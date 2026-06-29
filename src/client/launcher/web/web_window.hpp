#pragma once
#include "../window.hpp"
#include "web_frame.hpp"

// AlterBO3 (IKAAM): WebView2 counterpart to html_window. Owns the native
// window plus a web_frame, and forwards resize events to the controller.
class web_window final {
public:
  web_window(const std::string &title, int width, int height,
             long flags = WS_OVERLAPPEDWINDOW);

  ~web_window() = default;

  window *get_window();
  web_frame *get_web_frame();

  // Name kept identical to html_window so launcher.cpp can call either.
  web_frame *get_html_frame() { return this->get_web_frame(); }

private:
  web_frame frame_{};
  window window_;

  std::optional<LRESULT> processor(UINT message, WPARAM w_param,
                                   LPARAM l_param);
};
