#include "ui/window.h"

#if !defined(__APPLE__) && !defined(_WIN32)
void bm_window_setup(void *native_window) { (void)native_window; }
void bm_window_drag(void *native_window) { (void)native_window; }
void bm_window_minimize(void *native_window) { (void)native_window; }
void bm_window_maximize_toggle(void *native_window) { (void)native_window; }
void bm_window_close(void *native_window) { (void)native_window; }
bool bm_window_is_maximized(void *native_window) { (void)native_window; return false; }
bool bm_window_has_native_traffic_lights(void) { return false; }
#endif
