#include <std_include.hpp>
#include "loader/component_loader.hpp"

#include "splash.hpp"
#include "resource.hpp"

#include <utils/nt.hpp>
#include <utils/image.hpp>

namespace splash {
namespace {
HWND window{};
utils::image::object image{};
std::thread window_thread{};

utils::image::object load_splash_image() {
  // const auto self = utils::nt::library::get_by_address(load_splash_image);
  // return LoadImageA(self, MAKEINTRESOURCE(IMAGE_SPLASH), IMAGE_BITMAP, 0, 0,
  // LR_DEFAULTCOLOR);

  const auto res = utils::nt::load_resource(IMAGE_SPLASH);
  const auto img = utils::image::load_image(res);
  return utils::image::create_bitmap(img);
}

void destroy_window() {
  if (window && IsWindow(window)) {
    ShowWindow(window, SW_HIDE);
    DestroyWindow(window);
    window = nullptr;

    if (window_thread.joinable()) {
      window_thread.join();
    }

    window = nullptr;
  } else if (window_thread.joinable()) {
    window_thread.detach();
  }
}

void show() {
  WNDCLASSA wnd_class;

  const auto self = utils::nt::library::get_by_address(load_splash_image);

  wnd_class.style = CS_DROPSHADOW;
  wnd_class.cbClsExtra = 0;
  wnd_class.cbWndExtra = 0;
  wnd_class.lpszMenuName = nullptr;
  wnd_class.lpfnWndProc = DefWindowProcA;
  wnd_class.hInstance = self;
  wnd_class.hIcon = LoadIconA(self, MAKEINTRESOURCEA(ID_ICON));
  wnd_class.hCursor = LoadCursorA(nullptr, IDC_APPSTARTING);
  wnd_class.hbrBackground = reinterpret_cast<HBRUSH>(6);
  wnd_class.lpszClassName = "Black Ops III Splash Screen";

  if (RegisterClassA(&wnd_class)) {
    const auto x_pixels = GetSystemMetrics(SM_CXFULLSCREEN);
    const auto y_pixels = GetSystemMetrics(SM_CYFULLSCREEN);

    if (image) {
      window = CreateWindowExA(WS_EX_APPWINDOW, "Black Ops III Splash Screen",
                               "AlterBO3", WS_POPUP | WS_SYSMENU,
                               (x_pixels - 320) / 2, (y_pixels - 100) / 2, 320,
                               100, nullptr, nullptr, self, nullptr);

      if (window) {
        // AlterBO3 (IKAAM): SS_REALSIZECONTROL stretches the bitmap to the
        // static's size, so we can force a fixed display size regardless of
        // the source image resolution.
        auto *const image_window = CreateWindowExA(
            0, "Static", nullptr,
            WS_CHILD | WS_VISIBLE | SS_BITMAP | SS_REALSIZECONTROL, 0, 0, 320,
            100, window, nullptr, self, nullptr);
        if (image_window) {
          SendMessageA(image_window, STM_SETIMAGE, IMAGE_BITMAP, image);

          // Read the source bitmap dimensions to keep the aspect ratio.
          BITMAP bm{};
          GetObjectA(image.get(), sizeof(bm), &bm);
          const int src_w = bm.bmWidth > 0 ? bm.bmWidth : 16;
          const int src_h = bm.bmHeight > 0 ? bm.bmHeight : 9;

          // AlterBO3 (IKAAM): fixed display width, height scaled to ratio.
          const int target_w = 550;
          const int target_h = static_cast<int>(static_cast<double>(target_w) *
                                                static_cast<double>(src_h) /
                                                static_cast<double>(src_w));

          MoveWindow(image_window, 0, 0, target_w, target_h, TRUE);

          RECT rect;
          rect.left = (x_pixels - target_w) / 2;
          rect.top = (y_pixels - target_h) / 2;
          rect.right = rect.left + target_w;
          rect.bottom = rect.top + target_h;
          AdjustWindowRect(&rect, WS_CHILD | WS_VISIBLE | 0xEu, 0);
          SetWindowPos(window, nullptr, rect.left, rect.top,
                       rect.right - rect.left, rect.bottom - rect.top,
                       SWP_NOZORDER);

          SetWindowRgn(window,
                       CreateRoundRectRgn(0, 0, rect.right - rect.left,
                                          rect.bottom - rect.top, 15, 15),
                       TRUE);

          ShowWindow(window, SW_SHOW);
          UpdateWindow(window);
        }
      }
    }
  }
}

bool draw_frame() {
  if (!window) {
    return false;
  }

  MSG msg{};
  bool success = true;

  while (PeekMessageW(&msg, nullptr, NULL, NULL, PM_REMOVE)) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);

    if (msg.message == WM_DESTROY && msg.hwnd == window) {
      PostQuitMessage(0);
    }

    if (msg.message == WM_QUIT) {
      success = false;
    }
  }
  return success;
}

void draw() {
  show();
  while (draw_frame()) {
    std::this_thread::sleep_for(1ms);
  }

  window = nullptr;
  UnregisterClassA("Black Ops III Splash Screen", utils::nt::library{});
}
} // namespace

struct component final : client_component {
  component() {
    image = load_splash_image();
    window_thread = std::thread([] { draw(); });
  }

  void pre_destroy() override {
    destroy_window();
    if (window_thread.joinable()) {
      window_thread.detach();
    }
  }

  void post_unpack() override { destroy_window(); }
};

void hide() {
  if (window && IsWindow(window)) {
    ShowWindow(window, SW_HIDE);
    UpdateWindow(window);
  }

  destroy_window();
}

HWND get_window() { return window; }
} // namespace splash

REGISTER_COMPONENT(splash::component)