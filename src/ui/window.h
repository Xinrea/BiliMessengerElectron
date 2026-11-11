#ifndef BM_UI_WINDOW_H
#define BM_UI_WINDOW_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void bm_window_setup(void *native_window);
void bm_window_set_drag_rect(void *native_window, float x, float y, float w, float h);
void bm_window_drag(void *native_window);
void bm_window_minimize(void *native_window);
void bm_window_maximize_toggle(void *native_window);
void bm_window_close(void *native_window);
bool bm_window_is_maximized(void *native_window);
bool bm_window_has_native_traffic_lights(void);

#ifdef __cplusplus
}
#endif

#endif /* BM_UI_WINDOW_H */
