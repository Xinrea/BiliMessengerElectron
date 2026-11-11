#include "ui/window.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dwmapi.h>

void bm_window_setup(void *native_window) {
    if (!native_window) return;
    HWND hwnd = (HWND)native_window;
    LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    style &= ~WS_CAPTION;
    SetWindowLongPtrW(hwnd, GWL_STYLE, style);

    MARGINS margins = { 0, 0, 1, 0 };
    DwmExtendFrameIntoClientArea(hwnd, &margins);

    SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

void bm_window_drag(void *native_window) {
    if (!native_window) return;
    HWND hwnd = (HWND)native_window;
    ReleaseCapture();
    SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
}

void bm_window_minimize(void *native_window) {
    if (!native_window) return;
    ShowWindow((HWND)native_window, SW_MINIMIZE);
}

void bm_window_maximize_toggle(void *native_window) {
    if (!native_window) return;
    HWND hwnd = (HWND)native_window;
    ShowWindow(hwnd, IsZoomed(hwnd) ? SW_RESTORE : SW_MAXIMIZE);
}

void bm_window_close(void *native_window) {
    if (!native_window) return;
    PostMessageW((HWND)native_window, WM_CLOSE, 0, 0);
}

bool bm_window_is_maximized(void *native_window) {
    if (!native_window) return false;
    return IsZoomed((HWND)native_window);
}

bool bm_window_has_native_traffic_lights(void) {
    return false;
}
#endif
