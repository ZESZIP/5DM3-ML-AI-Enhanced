/** \file
 * Cinema OS — reference-matched VRAM GUI (mountain bg, crystal rows, status chrome).
 */
#include "dryos.h"
#include "bmp.h"
#include "font.h"
#include "battery.h"
#include "fio-ml.h"
#include "cinema_gui_engine.h"
#include "cinema_ui_theme.h"

#define CINE_GUI_CACHE_W  720
#define CINE_GUI_CACHE_H  480

static struct bmp_file_t * bg_gray = 0;
static struct bmp_file_t * cap_left = 0;
static struct bmp_file_t * cap_right = 0;
static struct bmp_file_t * cap_mid = 0;
static int gui_ready = 0;

static uint8_t * tinted_bg[CINE_PAGE_COUNT];
static int tinted_valid[CINE_PAGE_COUNT];

/* Bronze/copper crystal palette for CINE accent */
static int cine_gui_bronze(int level)
{
    switch (level)
    {
        case 0: return COLOR_YELLOW;
        case 1: return COLOR_ORANGE;
        case 2: return COLOR_DARK_ORANGE_MOD;
        default: return COLOR_DARK_GREEN2_MOD;
    }
}

static int cine_gui_accent_shade(int accent, int level)
{
    if (accent == COLOR_ORANGE)
        return level == 0 ? COLOR_YELLOW
             : level == 1 ? COLOR_ORANGE
             : COLOR_DARK_ORANGE_MOD;
    if (accent == COLOR_GREEN1)
        return level == 0 ? COLOR_GREEN1
             : level == 1 ? COLOR_DARK_GREEN1_MOD
             : COLOR_DARK_GREEN2_MOD;
    if (accent == COLOR_YELLOW)
        return level == 0 ? COLOR_YELLOW
             : level == 1 ? COLOR_DARK_ORANGE_MOD
             : COLOR_DARK_GREEN2_MOD;
    if (accent == COLOR_LIGHT_BLUE)
        return level == 0 ? COLOR_LIGHT_BLUE
             : level == 1 ? COLOR_BLUE
             : COLOR_DARK_CYAN1_MOD;
    if (accent == COLOR_MAGENTA)
        return level == 0 ? COLOR_MAGENTA
             : level == 1 ? COLOR_DARK_CYAN2_MOD
             : COLOR_DARK_CYAN1_MOD;
    return level == 0 ? accent : COLOR_GRAY(25 + level * 12);
}

static int cine_gui_lum_to_color(int lum, int accent)
{
    if (lum < 6) return COLOR_BLACK;
    if (lum < 55)  return cine_gui_accent_shade(accent, 2);
    if (lum < 150) return cine_gui_accent_shade(accent, 1);
    return cine_gui_accent_shade(accent, 0);
}

static int cine_gui_lum_to_bronze(int lum)
{
    if (lum < 6) return COLOR_BLACK;
    if (lum > 200) return cine_gui_bronze(0);
    if (lum > 100) return cine_gui_bronze(1);
    if (lum > 35)  return cine_gui_bronze(2);
    return COLOR_BLACK;
}

static void cine_gui_free_tint_cache(void)
{
    for (int i = 0; i < CINE_PAGE_COUNT; i++)
    {
        if (tinted_bg[i]) { free(tinted_bg[i]); tinted_bg[i] = 0; }
        tinted_valid[i] = 0;
    }
}

static void cine_gui_build_tinted_bg(cinema_os_page_t page)
{
    if (!bg_gray || !bg_gray->image) return;
    if (tinted_valid[page] && tinted_bg[page]) return;

    int accent = cinema_os_page_color(page);
    int bw = bg_gray->width;
    int bh = bg_gray->height;
    int cw = MIN(bw, CINE_GUI_CACHE_W);
    int ch = MIN(bh, CINE_GUI_CACHE_H);
    int bytes = cw * ch;
    if (bytes <= 0) return;

    if (tinted_bg[page]) free(tinted_bg[page]);
    tinted_bg[page] = malloc(bytes);
    if (!tinted_bg[page]) return;

    for (int y = 0; y < ch; y++)
    {
        int src_y = bh - 1 - (y * bh / ch);
        uint8_t * row = tinted_bg[page] + y * cw;
        for (int x = 0; x < cw; x++)
        {
            int src_x = x * bw / cw;
            int lum = bg_gray->image[src_x + src_y * bw];
            row[x] = (uint8_t) cine_gui_lum_to_color(lum, accent);
        }
    }
    tinted_valid[page] = 1;
}

static void cine_gui_blit_cache(int x, int y, int w, int h, const uint8_t * cache, int cw, int ch)
{
    uint8_t * bvram = bmp_vram();
    if (!bvram || !cache) return;

    for (int yy = 0; yy < h; yy++)
    {
        int sy = yy * ch / h;
        if (sy >= ch) sy = ch - 1;
        int ys = y + yy;
        if (ys < BMP_H_MINUS || ys >= BMP_H_PLUS) continue;
        uint8_t * dst = bvram + ys * BMPPITCH;
        const uint8_t * src = cache + sy * cw;
        for (int xx = 0; xx < w; xx++)
        {
            int sx = xx * cw / w;
            if (sx >= cw) sx = cw - 1;
            int xs = x + xx;
            if (xs < BMP_W_MINUS || xs >= BMP_W_PLUS) continue;
            uint8_t c = src[sx];
            if (c) dst[xs] = c;
        }
    }
}

static void cine_gui_blit_cap_tinted(
    struct bmp_file_t * cap, int x0, int y0, int w, int h, int use_bronze)
{
    if (!cap || !cap->image) return;
    uint8_t * bvram = bmp_vram();
    if (!bvram) return;

    for (int yy = 0; yy < h; yy++)
    {
        int sy = yy * cap->height / h;
        int ys = y0 + yy;
        if (ys < BMP_H_MINUS || ys >= BMP_H_PLUS) continue;
        uint8_t * dst = bvram + ys * BMPPITCH;

        for (int xx = 0; xx < w; xx++)
        {
            int sx = xx * cap->width / w;
            int xs = x0 + xx;
            if (xs < BMP_W_MINUS || xs >= BMP_W_PLUS) continue;

            int lum = cap->image[sx + (cap->height - 1 - sy) * cap->width];
            if (lum < 4) continue;
            dst[xs] = use_bronze ? cine_gui_lum_to_bronze(lum) : cine_gui_lum_to_bronze(lum);
        }
    }
}

static void cine_gui_draw_crystal_procedural(int x, int y, int w, int h)
{
    int hi = cine_gui_bronze(0);
    int mid = cine_gui_bronze(1);
    int lo = cine_gui_bronze(2);
    int point = 24;

    bmp_fill(lo, x + point, y, w - 2 * point, h);
    for (int i = 0; i < point; i++)
    {
        int slice_w = MAX(1, (point - i) * 2);
        bmp_fill(mid, x + i, y + i * h / (2 * point), slice_w, h - i * h / point);
        bmp_fill(mid, x + w - i - slice_w, y + i * h / (2 * point), slice_w, h - i * h / point);
    }
    bmp_draw_rect(hi, x + 2, y + 1, w - 4, h - 2);
    bmp_fill(hi, x + point, y, w - 2 * point, 2);
    bmp_fill(COLOR_BLACK, x + point + 6, y + 8, w - 2 * point - 12, h - 16);
    bmp_fill(lo, x + point + 8, y + 10, w - 2 * point - 16, h - 20);
    bmp_fill(COLOR_BLACK, x + point + 12, y + 12, w - 2 * point - 24, h - 24);
}

int cinema_gui_list_top_y(void)
{
    return CINE_HDR_H + CINE_NAV_H + 12;
}

void cinema_gui_init(void)
{
    if (gui_ready) return;

    bg_gray = bmp_load(CINE_GUI_BG_FILE, 0);
    cap_left = bmp_load(CINE_GUI_CAP_L_FILE, 0);
    cap_right = bmp_load(CINE_GUI_CAP_R_FILE, 0);
    cap_mid = bmp_load(CINE_GUI_CAP_M_FILE, 0);

    gui_ready = bg_gray && cap_left && cap_right;
    if (!gui_ready)
    {
        if (bg_gray) { free(bg_gray); bg_gray = 0; }
        if (cap_left) { free(cap_left); cap_left = 0; }
        if (cap_right) { free(cap_right); cap_right = 0; }
        if (cap_mid) { free(cap_mid); cap_mid = 0; }
        cine_gui_free_tint_cache();
        return;
    }

    for (int p = 0; p < CINE_PAGE_COUNT; p++)
        cine_gui_build_tinted_bg((cinema_os_page_t) p);
}

int cinema_gui_assets_ready(void)
{
    return gui_ready;
}

void cinema_gui_draw_full_background(cinema_os_page_t page)
{
    cinema_gui_draw_page_bg(page, 0, 0, 720, 480);
}

void cinema_gui_draw_page_bg(cinema_os_page_t page, int x, int y, int w, int h)
{
    if (!gui_ready)
    {
        cine_ui_draw_backdrop(x, y, w, h, cinema_os_page_color(page));
        return;
    }

    cine_gui_build_tinted_bg(page);
    if (tinted_valid[page] && tinted_bg[page])
        cine_gui_blit_cache(x, y, w, h, tinted_bg[page],
            MIN(bg_gray->width, CINE_GUI_CACHE_W), MIN(bg_gray->height, CINE_GUI_CACHE_H));
    else
        cine_ui_draw_backdrop(x, y, w, h, cinema_os_page_color(page));
}

void cinema_gui_draw_status_header(void)
{
    bmp_fill(COLOR_BLACK, 0, 0, 720, CINE_HDR_H);

    int batt = GetBatteryLevel();
    if (batt < 0) batt = 0;
    if (batt > 100) batt = 100;
    bmp_printf(FONT(FONT_SMALL, COLOR_WHITE, COLOR_BLACK), 16, 8, "%d%%", batt);

    bmp_printf(FONT(FONT_MED, COLOR_WHITE, COLOR_BLACK), 330, 6, "MENU");

    struct tm now;
    LoadCalendarFromRTC(&now);
    bmp_printf(FONT(FONT_SMALL, COLOR_WHITE, COLOR_BLACK), 620, 8,
        "%02d:%02d", now.tm_hour, now.tm_min);
}

void cinema_gui_draw_status_footer(void)
{
    int fy = 480 - CINE_FOOT_H;
    bmp_fill(COLOR_BLACK, 0, fy, 720, CINE_FOOT_H);

    int batt = GetBatteryLevel();
    if (batt < 0) batt = 0;
    bmp_printf(FONT(FONT_SMALL, COLOR_WHITE, COLOR_BLACK), 16, fy + 6, "%d%%", batt);

    struct tm now;
    LoadCalendarFromRTC(&now);
    bmp_printf(FONT(FONT_SMALL, COLOR_WHITE, COLOR_BLACK), 330, fy + 6,
        "%02d:%02d", now.tm_hour, now.tm_min);

    struct card_info * card = get_shooting_card();
    if (card && card->type && card->free_space_raw > 0)
    {
        int gb = card->free_space_raw / 1024;
        bmp_printf(FONT(FONT_SMALL, COLOR_WHITE, COLOR_BLACK), 500, fy + 6,
            "%s: %dGB", card->type, gb);
    }
}

void cinema_gui_draw_crystal_row(int x, int y, int w, int h, int accent, int scale_pct)
{
    (void) accent;
    if (scale_pct != 100)
    {
        int nw = w * scale_pct / 100;
        int nh = h * scale_pct / 100;
        x -= (nw - w) / 2;
        y -= (nh - h) / 2;
        w = nw;
        h = nh;
    }

    int cap_w = gui_ready ? cap_left->width : 28;
    int inset = cap_w - 2;
    if (inset < 20) inset = 20;
    if (inset * 2 >= w) inset = w / 4;

    if (gui_ready)
    {
        cine_gui_blit_cap_tinted(cap_left, x, y, inset, h, 1);
        if (cap_mid)
        {
            int mid_w = w - 2 * inset;
            int tile = cap_mid->width;
            int mx = x + inset;
            while (mid_w > 0)
            {
                int chunk = MIN(mid_w, tile);
                cine_gui_blit_cap_tinted(cap_mid, mx, y, chunk, h, 1);
                mx += chunk;
                mid_w -= chunk;
            }
        }
        else
            bmp_fill(cine_gui_bronze(1), x + inset, y + 2, w - 2 * inset, h - 4);

        cine_gui_blit_cap_tinted(cap_right, x + w - inset, y, inset, h, 1);
    }
    else
    {
        cine_gui_draw_crystal_procedural(x, y, w, h);
        return;
    }

    int hi = cine_gui_bronze(0);
    bmp_draw_rect(hi, x + 2, y + 1, w - 4, h - 2);
    bmp_fill(hi, x + inset, y, w - 2 * inset, 2);

    int pad = 10;
    bmp_fill(COLOR_BLACK, x + inset, y + pad, w - 2 * inset, h - 2 * pad);
}

void cinema_gui_draw_text_shadow(int font_spec, int x, int y, const char * text, int fg)
{
    bmp_printf(FONT(font_spec, COLOR_GRAY(5), NO_BG_ERASE), x + 2, y + 2, "%s", text);
    bmp_printf(FONT(font_spec, fg, NO_BG_ERASE), x, y, "%s", text);
}

void cinema_gui_draw_text_shadow_r(int font_spec, int x, int y, int w, const char * text, int fg)
{
    int tw = bmp_string_width(FONT(font_spec, fg, NO_BG_ERASE), text);
    int rx = x + w - tw - 8;
    if (rx < x) rx = x;
    cinema_gui_draw_text_shadow(font_spec, rx, y, text, fg);
}

void cinema_gui_draw_menu_row(
    int x, int y, int w, int row_h,
    int accent, int selected,
    int icon, const char * label, const char * value)
{
    if (selected)
    {
        int scale = 115;
        int nh = row_h * scale / 100;
        int ny = y - (nh - row_h) / 2;
        cinema_gui_draw_crystal_row(x, ny, w, nh, accent, scale);

        if (icon)
            bfnt_draw_char(icon, x + 18, ny + (nh - 28) / 2, COLOR_WHITE, NO_BG_ERASE);

        int tx = icon ? x + 54 : x + 20;
        cinema_gui_draw_text_shadow(FONT_CANON, tx, ny + (nh - font_large.height) / 2, label, COLOR_WHITE);
        if (value && value[0])
            cinema_gui_draw_text_shadow_r(FONT_MED, x, ny + (nh - font_med.height) / 2, w - 24, value, COLOR_WHITE);
    }
    else
    {
        cinema_gui_draw_text_shadow(FONT_LARGE, x + 20, y + (row_h - font_large.height) / 2, label, COLOR_WHITE);
        if (value && value[0])
            cinema_gui_draw_text_shadow_r(FONT_MED, x, y + (row_h - font_med.height) / 2, w - 16, value, COLOR_WHITE);
    }
}

void cinema_gui_draw_nav_bar(
    int y, int bar_h, int active_page,
    const int * page_colors, const char ** labels, const int * icons, int page_count)
{
    cine_ui_draw_matte_nav_bar(y, bar_h, active_page, page_colors, labels, icons, page_count);
}

void cinema_gui_draw_scrollbar(int x, int y, int h, int total, int visible, int scroll, int accent)
{
    if (total <= visible) return;
    bmp_fill(COLOR_GRAY(15), x, y, 4, h);
    int thumb_h = MAX(18, h * visible / total);
    int max_scroll = total - visible;
    int thumb_y = y + (h - thumb_h) * scroll / MAX(1, max_scroll);
    bmp_fill(accent, x, thumb_y, 4, thumb_h);
}
