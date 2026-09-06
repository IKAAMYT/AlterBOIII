#pragma once
#include "../window.hpp"
#include "html_frame.hpp"

class html_window final {
public:
  html_window(const std::string &title, int width, int height,
              long flags = WS_OVERLAPPEDWINDOW);

  ~html_window() = default;

  window *get_window();
  html_frame *get_html_frame();

private:
  html_frame frame_{};
  window window_;
  int sync_ticks_ = 0;

  void sync_frame_size();

  std::optional<LRESULT> processor(UINT message, WPARAM w_param,
                                   LPARAM l_param);
};