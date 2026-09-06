#include <std_include.hpp>
#include "html_window.hpp"

namespace {
// Largeur de la zone sensible au redimensionnement, en pixels.
constexpr int RESIZE_BORDER = 8;

// Identifiant du timer de rattrapage de taille.
constexpr UINT_PTR SYNC_TIMER_ID = 1;

// Presentes depuis le SDK Windows 11 ; redefinies ici pour compiler avec un
// SDK plus ancien.
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
constexpr DWORD DWMWA_WINDOW_CORNER_PREFERENCE_COMPAT = 33;
#else
constexpr DWORD DWMWA_WINDOW_CORNER_PREFERENCE_COMPAT =
    DWMWA_WINDOW_CORNER_PREFERENCE;
#endif
constexpr DWORD DWMWCP_ROUND_COMPAT = 2;

int frame_border(const HWND handle) {
  const UINT dpi = GetDpiForWindow(handle);
  return GetSystemMetricsForDpi(SM_CXFRAME, dpi) +
         GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
}
} // namespace

html_window::html_window(const std::string &title, int width, int height,
                         long flags)
    : window_(
          title, width, height,
          [this](window *, const UINT message, const WPARAM w_param,
                 const LPARAM l_param) -> std::optional<LRESULT> {
            return this->processor(message, w_param, l_param);
          },
          flags) {}

window *html_window::get_window() { return &this->window_; }

html_frame *html_window::get_html_frame() { return &this->frame_; }

void html_window::sync_frame_size() {
  const HWND handle = this->window_;
  if (!handle) {
    return;
  }

  RECT client{};
  if (GetClientRect(handle, &client)) {
    this->frame_.resize(static_cast<DWORD>(client.right - client.left),
                        static_cast<DWORD>(client.bottom - client.top));
  }
}

std::optional<LRESULT> html_window::processor(const UINT message,
                                              const WPARAM w_param,
                                              const LPARAM l_param) {
  const HWND handle = this->window_;

  if (message == WM_ERASEBKGND) {
    return 1;
  }

  // AlterBOIII : fenetre sans cadre natif.
  //
  // On garde WS_OVERLAPPEDWINDOW (donc l'ancrage Windows, les animations de
  // reduction et l'ombre portee) mais on supprime la zone non-cliente, ce qui
  // fait disparaitre la barre de titre. La page HTML occupe alors toute la
  // fenetre et fournit ses propres boutons.
  if (message == WM_NCCALCSIZE && w_param == TRUE && handle) {
    auto *params = reinterpret_cast<NCCALCSIZE_PARAMS *>(l_param);

    // Maximisee, la fenetre deborderait de l'ecran de la largeur du cadre
    // invisible : on la retracte manuellement.
    if (IsZoomed(handle)) {
      const int border = frame_border(handle);
      params->rgrc[0].left += border;
      params->rgrc[0].right -= border;
      params->rgrc[0].top += border;
      params->rgrc[0].bottom -= border;
    }

    return 0;
  }

  // Sans zone non-cliente, Windows ne sait plus ou sont les bords : on lui
  // indique nous-memes, sinon la fenetre n'est plus redimensionnable.
  if (message == WM_NCHITTEST && handle) {
    if (IsZoomed(handle)) {
      return HTCLIENT;
    }

    RECT rect{};
    if (!GetWindowRect(handle, &rect)) {
      return HTCLIENT;
    }

    const auto x = static_cast<int>(static_cast<short>(LOWORD(l_param)));
    const auto y = static_cast<int>(static_cast<short>(HIWORD(l_param)));

    const bool gauche = x < rect.left + RESIZE_BORDER;
    const bool droite = x >= rect.right - RESIZE_BORDER;
    const bool haut = y < rect.top + RESIZE_BORDER;
    const bool bas = y >= rect.bottom - RESIZE_BORDER;

    if (haut && gauche)
      return HTTOPLEFT;
    if (haut && droite)
      return HTTOPRIGHT;
    if (bas && gauche)
      return HTBOTTOMLEFT;
    if (bas && droite)
      return HTBOTTOMRIGHT;
    if (haut)
      return HTTOP;
    if (bas)
      return HTBOTTOM;
    if (gauche)
      return HTLEFT;
    if (droite)
      return HTRIGHT;

    // Le reste appartient a la page : c'est elle qui declenche le
    // deplacement via le callback winDrag.
    return HTCLIENT;
  }

  // La taille vient du rect client reel, pas du l_param : apres
  // WM_NCCALCSIZE la zone client couvre toute la fenetre, et c'est la
  // seule source fiable.
  if ((message == WM_SIZE || message == WM_WINDOWPOSCHANGED) && handle) {
    this->sync_frame_size();
    if (message == WM_SIZE) {
      return 0;
    }
    return {};
  }

  // html_frame::resize() ne fait rien tant que le controleur WebView2
  // n'existe pas, et sa creation est ASYNCHRONE : les WM_SIZE du demarrage
  // arrivent trop tot. Sans ce rattrapage, la WebView garde ses bornes
  // initiales et le pinceau de fond de la fenetre apparait sur les bords.
  if (message == WM_TIMER && w_param == SYNC_TIMER_ID) {
    this->sync_frame_size();
    if (++this->sync_ticks_ >= 12) {
      KillTimer(handle, SYNC_TIMER_ID);
    }
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

    // Coins arrondis Windows 11. Ignore silencieusement ailleurs.
    DWORD arrondi = DWMWCP_ROUND_COMPAT;
    DwmSetWindowAttribute(this->window_, DWMWA_WINDOW_CORNER_PREFERENCE_COMPAT,
                          &arrondi, sizeof(arrondi));

    // Force un recalcul de la zone non-cliente pour que WM_NCCALCSIZE
    // s'applique des l'affichage, sans clignotement de barre de titre.
    SetWindowPos(this->window_, nullptr, 0, 0, 0, 0,
                 SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                     SWP_NOACTIVATE);

    // Re-synchronise pendant ~3 s, le temps que WebView2 se cree.
    SetTimer(this->window_, SYNC_TIMER_ID, 250, nullptr);
    return 0;
  }

  return {};
}
