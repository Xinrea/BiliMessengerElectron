#ifndef BM_IMAGES_H
#define BM_IMAGES_H

#include <cui/cui.h>
#include <stdbool.h>

void bm_images_init(void);
void bm_images_shutdown(cui_ctx *ui);

cui_texture bm_image_get(cui_ctx *ui, const char *url, int *out_w, int *out_h);
cui_texture bm_qr_create_texture(cui_ctx *ui, const char *text, int *out_px);

#endif /* BM_IMAGES_H */
