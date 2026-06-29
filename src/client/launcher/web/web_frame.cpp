#include <std_include.hpp>
#include "web_frame.hpp"

#include <wrl/event.h>

#include <utils/string.hpp>
#include <utils/nt.hpp>
#include <game/game.hpp>

#pragma comment(lib, "version.lib")

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

namespace {
// UTF-8 <-> UTF-16 helpers (WebView2 APIs are all wide-char).
std::wstring widen(const std::string &str) {
  if (str.empty())
    return {};
  const int size = MultiByteToWideChar(CP_UTF8, 0, str.data(),
                                       static_cast<int>(str.size()), nullptr, 0);
  std::wstring result(static_cast<size_t>(size), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()),
                      result.data(), size);
  return result;
}

std::string narrow(const std::wstring &str) {
  if (str.empty())
    return {};
  const int size = WideCharToMultiByte(CP_UTF8, 0, str.data(),
                                       static_cast<int>(str.size()), nullptr, 0,
                                       nullptr, nullptr);
  std::string result(static_cast<size_t>(size), '\0');
  WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()),
                      result.data(), size, nullptr, nullptr);
  return result;
}

std::string narrow(LPCWSTR str) {
  if (!str)
    return {};
  return narrow(std::wstring(str));
}

// Minimal JSON string escaper for the injected shim and outgoing messages.
std::string json_escape(const std::string &input) {
  std::string out;
  out.reserve(input.size() + 8);
  for (const char c : input) {
    switch (c) {
    case '"':
      out += "\\\"";
      break;
    case '\\':
      out += "\\\\";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      if (static_cast<unsigned char>(c) < 0x20) {
        char buf[8];
        sprintf_s(buf, "\\u%04x", c);
        out += buf;
      } else {
        out += c;
      }
      break;
    }
  }
  return out;
}

// Convert a CComVariant result into a JSON literal for the JS side.
std::string variant_to_json(const CComVariant &value) {
  switch (value.vt) {
  case VT_BSTR: {
    const std::wstring wide(value.bstrVal ? value.bstrVal : L"");
    return "\"" + json_escape(narrow(wide)) + "\"";
  }
  case VT_I4:
    return std::to_string(value.intVal);
  case VT_BOOL:
    return value.boolVal != FALSE ? "true" : "false";
  case VT_EMPTY:
  case VT_NULL:
  default:
    return "null";
  }
}
} // namespace

web_frame::web_frame() = default;

web_frame::~web_frame() {
  if (this->controller_) {
    this->controller_->Close();
  }
}

HWND web_frame::get_window() const { return this->window_; }

void web_frame::register_callback(
    const std::string &name,
    const std::function<CComVariant(const std::vector<html_argument> &)>
        &callback) {
  this->callbacks_.emplace_back(name, callback);
}

html_argument
web_frame::invoke_callback(const std::string &name,
                           const std::vector<html_argument> &params) const {
  for (const auto &entry : this->callbacks_) {
    if (entry.first == name) {
      return entry.second(params);
    }
  }
  return {};
}

void web_frame::initialize(const HWND window) {
  this->window_ = window;

  // CreateCoreWebView2EnvironmentWithOptions resolves the loader statically
  // (WebView2LoaderStatic.lib). The user-data folder is placed under the
  // launcher's appdata so it survives reinstalls but stays per-app.
  const std::wstring user_data =
      (game::get_appdata_path() / "webview2").wstring();

  const HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
      nullptr, user_data.c_str(), nullptr,
      Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
          [this](HRESULT result, ICoreWebView2Environment *env) -> HRESULT {
            if (SUCCEEDED(result) && env) {
              this->on_environment_ready(env);
            }
            return S_OK;
          })
          .Get());

  if (FAILED(hr)) {
    // Caller is responsible for falling back to the legacy IE host.
    throw std::runtime_error(
        "CreateCoreWebView2EnvironmentWithOptions failed (WebView2 runtime "
        "missing?)");
  }
}

void web_frame::on_environment_ready(ICoreWebView2Environment *env) {
  this->environment_ = env;

  env->CreateCoreWebView2Controller(
      this->window_,
      Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
          [this](HRESULT result,
                 ICoreWebView2Controller *controller) -> HRESULT {
            if (SUCCEEDED(result) && controller) {
              this->on_controller_ready(controller);
            }
            return S_OK;
          })
          .Get());
}

void web_frame::on_controller_ready(ICoreWebView2Controller *controller) {
  this->controller_ = controller;
  this->controller_->get_CoreWebView2(&this->webview_);

  // Fit the webview to the host window's client area.
  RECT bounds{};
  GetClientRect(this->window_, &bounds);
  this->controller_->put_Bounds(bounds);

  this->wire_web_message_handler();
  this->inject_bridge_shim();

  this->ready_.store(true);
  this->apply_pending_url();
}

void web_frame::wire_web_message_handler() {
  if (!this->webview_)
    return;

  this->webview_->add_WebMessageReceived(
      Callback<ICoreWebView2WebMessageReceivedEventHandler>(
          [this](ICoreWebView2 *,
                 ICoreWebView2WebMessageReceivedEventArgs *args) -> HRESULT {
            LPWSTR raw = nullptr;
            if (FAILED(args->TryGetWebMessageAsString(&raw)) || !raw) {
              return S_OK;
            }
            const std::string message = narrow(std::wstring(raw));
            CoTaskMemFree(raw);

            // Parse {id, method, args:[...]} with rapidjson.
            rapidjson::Document doc;
            if (doc.Parse(message.c_str()).HasParseError() ||
                !doc.IsObject()) {
              return S_OK;
            }

            const int call_id =
                doc.HasMember("id") && doc["id"].IsInt() ? doc["id"].GetInt()
                                                         : -1;
            const std::string method =
                doc.HasMember("method") && doc["method"].IsString()
                    ? doc["method"].GetString()
                    : std::string{};
            if (method.empty()) {
              return S_OK;
            }

            std::vector<html_argument> params;
            if (doc.HasMember("args") && doc["args"].IsArray()) {
              for (auto &v : doc["args"].GetArray()) {
                if (v.IsString()) {
                  params.emplace_back(CComVariant(widen(v.GetString()).c_str()));
                } else if (v.IsBool()) {
                  params.emplace_back(CComVariant(v.GetBool()));
                } else if (v.IsInt()) {
                  params.emplace_back(CComVariant(v.GetInt()));
                } else if (v.IsDouble()) {
                  params.emplace_back(
                      CComVariant(static_cast<int>(v.GetDouble())));
                } else {
                  params.emplace_back(html_argument{});
                }
              }
            }

            const html_argument result =
                this->invoke_callback(method, params);

            // Post the result back so the JS Promise can resolve.
            if (call_id >= 0 && this->webview_) {
              const std::string reply =
                  "{\"__alterbo3_reply\":true,\"id\":" +
                  std::to_string(call_id) +
                  ",\"result\":" + variant_to_json(result.get()) + "}";
              this->webview_->PostWebMessageAsJson(widen(reply).c_str());
            }

            return S_OK;
          })
          .Get(),
      &this->web_message_token_);
}

std::string web_frame::build_bridge_shim() const {
  // Build the list of method names registered on the C++ side.
  std::string names;
  for (size_t i = 0; i < this->callbacks_.size(); ++i) {
    if (i)
      names += ",";
    names += "\"" + json_escape(this->callbacks_[i].first) + "\"";
  }

  // The shim recreates window.external.<name>(...) as Promise-returning
  // functions. Each call posts {id, method, args} and resolves when the
  // matching {__alterbo3_reply, id, result} comes back.
  std::string shim;
  shim += "(function(){";
  shim += "if (window.__alterbo3_bridge_ready) return;";
  shim += "window.__alterbo3_bridge_ready = true;";
  shim += "var __pending = {}; var __seq = 1;";
  shim += "window.chrome.webview.addEventListener('message', function(e){";
  shim += "  var d = e.data;";
  shim += "  if (d && d.__alterbo3_reply && __pending[d.id]) {";
  shim += "    __pending[d.id](d.result); delete __pending[d.id];";
  shim += "  }";
  shim += "});";
  shim += "function __call(method, args){";
  shim += "  return new Promise(function(resolve){";
  shim += "    var id = __seq++; __pending[id] = resolve;";
  shim += "    window.chrome.webview.postMessage(JSON.stringify({";
  shim += "      id:id, method:method, args:Array.prototype.slice.call(args)";
  shim += "    }));";
  shim += "  });";
  shim += "}";
  shim += "var __names = [" + names + "];";
  shim += "window.external = window.external || {};";
  shim += "__names.forEach(function(n){";
  shim += "  window.external[n] = function(){ return __call(n, arguments); };";
  shim += "});";
  shim += "})();";
  return shim;
}

void web_frame::inject_bridge_shim() {
  if (!this->webview_)
    return;

  const std::string shim = this->build_bridge_shim();

  // Runs on every document creation (before page scripts), so window.external
  // is always available by the time the UI's main.js executes.
  this->webview_->AddScriptToExecuteOnDocumentCreated(
      widen(shim).c_str(), nullptr);
}

void web_frame::apply_pending_url() {
  if (!this->pending_url_.empty()) {
    this->load_url(this->pending_url_);
    this->pending_url_.clear();
  }
}

void web_frame::resize(const DWORD width, const DWORD height) const {
  if (!this->controller_)
    return;
  RECT bounds{0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
  this->controller_->put_Bounds(bounds);
}

bool web_frame::load_url(const std::string &url) const {
  if (!this->webview_) {
    // Not ready yet: stash it (cast away const, mirrors html_frame's laziness).
    const_cast<web_frame *>(this)->pending_url_ = url;
    return true;
  }
  return SUCCEEDED(this->webview_->Navigate(widen(url).c_str()));
}

bool web_frame::load_html(const std::string &html) const {
  if (!this->webview_)
    return false;
  return SUCCEEDED(this->webview_->NavigateToString(widen(html).c_str()));
}

html_argument web_frame::evaluate(const std::string & /*javascript*/) const {
  // WebView2's ExecuteScript is asynchronous and returns via callback, so it
  // cannot map onto html_frame's synchronous evaluate(). The launcher pushes
  // data into the page through registered callbacks instead, so this is a
  // deliberate no-op kept only for API parity.
  return {};
}
