#include "ui/fonts.h"

#include <stdio.h>
#include <stdlib.h>

static const cui_strings g_zh_strings = {
    .month = {
        "一月", "二月", "三月", "四月", "五月", "六月",
        "七月", "八月", "九月", "十月", "十一月", "十二月"
    },
    .month_short = {
        "1月", "2月", "3月", "4月", "5月", "6月",
        "7月", "8月", "9月", "10月", "11月", "12月"
    },
    .weekday = {
        "星期一", "星期二", "星期三", "星期四", "星期五", "星期六", "星期日"
    },
    .weekday_short = {
        "周一", "周二", "周三", "周四", "周五", "周六", "周日"
    },
    .weekday_min = {
        "一", "二", "三", "四", "五", "六", "日"
    },
    .today = "今天"
};

static bool try_attach_cjk_font(cui_ctx *ui, const char *path) {
    if (!path || !path[0]) return false;
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    fclose(f);

    cui_font cjk = cui_font_load_file(ui, path);
    if (cjk < 0) return false;

    cui_text_metrics m = cui_measure_text(ui, cjk, 14.0f, "舰长", -1);
    if (m.width <= 0.0f) return false;

    cui_font_add_fallback(ui, CUI_FONT_BODY, cjk);
    cui_font_add_fallback(ui, CUI_FONT_MEDIUM, cjk);
    cui_font_add_fallback(ui, CUI_FONT_DISPLAY, cjk);
    cui_font_add_fallback(ui, CUI_FONT_MONO, cjk);
    return true;
}

bool bm_fonts_init(cui_ctx *ui) {
    cui_set_strings(ui, &g_zh_strings);

    const char *env_font = getenv("BM_FONT_PATH");
    if (env_font && try_attach_cjk_font(ui, env_font)) {
        return true;
    }

    static const char *candidates[] = {
#if defined(__APPLE__)
        "/System/Library/Fonts/STHeiti Medium.ttc",
        "/System/Library/Fonts/STHeiti Light.ttc",
        "/System/Library/Fonts/Hiragino Sans GB.ttc",
        "/System/Library/Fonts/PingFang.ttc",
        "/System/Library/Fonts/Supplemental/Songti.ttc",
        "/System/Library/Fonts/Supplemental/Arial Unicode.ttf",
        "/Library/Fonts/Arial Unicode.ttf",
#elif defined(_WIN32)
        "C:\\Windows\\Fonts\\msyh.ttc",
        "C:\\Windows\\Fonts\\msyh.ttf",
        "C:\\Windows\\Fonts\\simhei.ttf",
        "C:\\Windows\\Fonts\\simsun.ttc",
        "C:\\Windows\\Fonts\\deng.ttf",
#else
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",
        "/usr/share/fonts/wenquanyi/wqy-microhei/wqy-microhei.ttc",
        "/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf",
#endif
        NULL
    };

    for (int i = 0; candidates[i]; i++) {
        if (try_attach_cjk_font(ui, candidates[i])) {
            return true;
        }
    }
    return false;
}
