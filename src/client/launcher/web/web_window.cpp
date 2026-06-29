#include <std_include.hpp>
#include "web_window.hpp"

web_window::web_window(const std::string &title, int width, int height,
                       long flags)
    : window_(
          title, width, height,
          [this](window *, const UINT message, const WPARAM w_param,
                 const LPARAM l_param) -> std::optional<LRESULT> {
            return this->processor(message, w_param, l_param);
          },
          flags) {}

window *web_window::get_window() { return &this->window_; }

web_frame *web_window::get_web_frame() { return &this->frame_; }

std::optional<LRESULT> web_window::processor(const UINT message,
                                             const WPARAM /*w_param*/,
                                             const LPARAM l_param) {
  if (message == WM_ERASEBKGND) {
    return 1;
  }

  if (message == WM_SIZE) {
    this->frame_.resize(LOWORD(l_param), HIWORD(l_param));
    return 0;
  }

  if (message == WM_GETMINMAXINFO) {
    auto *mmi = reinterpret_cast<MINMAXINFO *>(l_param);
    mmi->ptMinTrackSize.x = 900;
    mmi->ptMinTrackSize.y = 500;
    return 0;
  }

  if (message == WM_CREATE) {
    this->frame_.initialize(this->window_);
    return 0;
  }

  return {};
}
