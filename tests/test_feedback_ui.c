/* Headless regression: exercise the real feedback dialog at supported window sizes. */
#include "ui/app.h"
#include <cui/cui_render.h>

#ifdef NDEBUG
#error "Feedback UI tests require assertions enabled"
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The dialog's asynchronous notification sink is unused: this test never submits. */
void bm_toast_push(bm_app *app, const char *title, const char *desc, cui_tone tone) {
    (void)app; (void)title; (void)desc; (void)tone;
    assert(!"Unexpected submission in UI test");
}

static void frame(cui_ctx *ui, bm_app *app, int frames) {
    for (int i = 0; i < frames; i++) {
        cui_begin_frame(ui, 1.0f / 60.0f);
        bm_render_feedback_dialog(ui, app);
        cui_end_frame(ui);
    }
}

/* CUI scopes dialog children under the modal ID (pinned CUI implementation). */
static cui_rect dialog_rect(cui_ctx *ui, const char *label) {
    cui_id dialog = cui_make_id(ui, "问题反馈") ^ 0xD1A106u;
    cui_push_id_int(ui, (int)dialog);
    cui_id id = cui_make_id(ui, label);
    cui_pop_id(ui);
    return cui_get_rect(ui, id);
}

static void click(cui_ctx *ui, bm_app *app, cui_rect rect) {
    assert(rect.w > 0 && rect.h > 0);
    cui_input_mouse_move(ui, rect.x + rect.w / 2, rect.y + rect.h / 2);
    frame(ui, app, 1);
    cui_input_mouse_button(ui, CUI_MOUSE_LEFT, true);
    frame(ui, app, 1);
    cui_input_mouse_button(ui, CUI_MOUSE_LEFT, false);
    frame(ui, app, 2);
}

static void test_reader(int width, int height, cui_theme_mode theme) {
    cui_sw_renderer *renderer = cui_sw_create();
    cui_ctx *ui = cui_create(&(cui_ctx_desc){.renderer = cui_sw_iface(renderer), .theme = theme});
    assert(ui);
    cui_set_viewport(ui, (float)width, (float)height, 1);
    cui_set_reduced_motion(ui, true);
    bm_app *app = (bm_app *)calloc(1, sizeof *app);
    assert(app);
    app->feedback_dialog = app->feedback_log_preview = app->feedback.include_log = true;
    strcpy(app->feedback.message, "Draft stays intact");
    for (int i = 0; i < 240; i++) {
        char line[200];
        snprintf(line, sizeof line,
                 "line=%03d [INFO] app op=config_save code=0 count=0 long line wraps without truncation\n", i);
        strcat(app->feedback.log, line);
    }
    strcat(app->feedback.log, "END_OF_LOG\n");
    size_t original_size = strlen(app->feedback.log);
    frame(ui, app, 4);
    cui_rect reader = dialog_rect(ui, "feedback_log_reader");
    cui_rect text = dialog_rect(ui, "feedback_log_text");
    cui_rect back = dialog_rect(ui, "返回反馈");
    cui_rect toolbar = dialog_rect(ui, "feedback_log_toolbar");
    cui_rect close = dialog_rect(ui, "关闭");
    assert(toolbar.h > 0 && toolbar.y + toolbar.h <= reader.y);
    assert(fabsf(close.y - back.y) < 1);
    assert(fabsf(back.x - (close.x + close.w) - 8) < 1);
    assert(reader.h > 300 && reader.y > 0);
    assert(text.h > reader.h * 5);
    assert(back.y >= reader.y + reader.h && back.y + back.h < height);

    /* Wheel over the log, not the outer dialog, must reveal its final line. */
    cui_input_mouse_move(ui, reader.x + reader.w / 2, reader.y + reader.h / 2);
    frame(ui, app, 1);
    cui_input_scroll(ui, 0, -10000);
    frame(ui, app, 4);
    cui_rect bottom = dialog_rect(ui, "feedback_log_text");
    assert(bottom.y < text.y);
    assert(bottom.y + bottom.h <= reader.y + reader.h + 1);
    assert(bottom.y + bottom.h >= reader.y + reader.h - 20);
    assert(fabsf(dialog_rect(ui, "返回反馈").y - back.y) < 1);
    assert(fabsf(dialog_rect(ui, "feedback_log_toolbar").y - toolbar.y) < 1);

    click(ui, app, back);
    assert(!app->feedback_log_preview && app->feedback_dialog);
    assert(!strcmp(app->feedback.message, "Draft stays intact"));
    assert(strlen(app->feedback.log) == original_size);
    assert(dialog_rect(ui, "feedback_form").h > 300);
    click(ui, app, dialog_rect(ui, "关闭"));
    assert(!app->feedback_dialog);
    app->feedback_dialog = true;
    app->feedback_log_preview = true;
    frame(ui, app, 4);
    assert(strlen(app->feedback.log) == original_size);
    assert(strstr(app->feedback.log, "END_OF_LOG\n"));
    free(app);
    cui_destroy(ui);
    cui_sw_destroy(renderer);
}

int main(void) {
    test_reader(1024, 640, CUI_THEME_LIGHT);
    test_reader(1280, 760, CUI_THEME_DARK);
    puts("PASS full log scrolling, visible footer, return and reopen without losing draft/snapshot");
    return 0;
}
