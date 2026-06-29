#pragma once

// AlterBO3 (IKAAM): WebView2 (Chromium/Edge) replacement for the legacy
// MSHTML/IE host (see ../html/html_frame.*). This class exposes the SAME public
// API as html_frame so that launcher.cpp can drive either backend without
// changing a single register_callback() call.
//
// The C++<->JS bridge works as follows:
//   * On document creation we inject a small JS shim that rebuilds
//     `window.external.<method>(...)` for every registered callback.
//   * Each JS call serialises {id, method, args} to JSON and posts it to C++
//     via window.chrome.webview.postMessage(...).
//   * C++ (WebMessageReceived) parses the JSON, runs the matching callback,
//     and posts the result back as JSON {id, result}.
//   * The shim resolves the pending Promise for that id.
//
// Synchronous callers on the JS side simply `await` the returned Promise.

#include <wrl/client.h>
#include "WebView2.h"

#include "../html/html_argument.hpp"

class web_frame {
public:
  web_frame();
  web_frame(const web_frame &) = delete;
  web_frame &operator=(const web_frame &) = delete;
  web_frame(web_frame &&) = delete;
  web_frame &operator=(web_frame &&) = delete;

  ~web_frame();

  // Same surface as html_frame -----------------------------------------------
  void initialize(HWND window);

  void resize(DWORD width, DWORD height) const;
  bool load_url(const std::string &url) const;
  bool load_html(const std::string &html) const;

  html_argument evaluate(const std::string &javascript) const;

  HWND get_window() const;

  void register_callback(
      const std::string &name,
      const std::function<CComVariant(const std::vector<html_argument> &)>
          &callback);

  // True once the underlying ICoreWebView2 controller is live.
  bool is_ready() const { return this->ready_.load(); }

private:
  HWND window_ = nullptr;
  std::atomic<bool> ready_{false};

  // Pending URL requested before the controller finished initializing.
  std::string pending_url_;

  Microsoft::WRL::ComPtr<ICoreWebView2Environment> environment_;
  Microsoft::WRL::ComPtr<ICoreWebView2Controller> controller_;
  Microsoft::WRL::ComPtr<ICoreWebView2> webview_;

  EventRegistrationToken web_message_token_{};

  std::vector<std::pair<std::string, std::function<html_argument(
                                         const std::vector<html_argument> &)>>>
      callbacks_;

  void on_environment_ready(ICoreWebView2Environment *env);
  void on_controller_ready(ICoreWebView2Controller *controller);
  void wire_web_message_handler();
  void inject_bridge_shim();
  void apply_pending_url();

  html_argument invoke_callback(const std::string &name,
                                const std::vector<html_argument> &params) const;

  // Builds the JS shim that recreates window.external.* from callbacks_.
  std::string build_bridge_shim() const;
};
