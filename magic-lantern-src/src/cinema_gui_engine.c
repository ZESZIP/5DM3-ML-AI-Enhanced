/** \file
 * Cinema OS 2026 — VRAM GUI engine: BMP assets, accent tinting, crystal highlighter.
 */
#include "dryos.h"
#include "bmp.h"
#include "font.h"
#include "cinema_gui_engine.h"
#include "cinema_ui_theme.h"

#define CINE_GUI_CACHE_W  720
#define CINE_GUI_CACHE_H  400

static struct bmp_file_t * bg_gray = 0;
static struct bmp_file_t * cap_left = 0;
static struct bmp_file_t * cap_right = 0;
static int gui_ready = 0;

static uint8_t * tinted_bg[CINE_PAGE_COUNT];
static int tinted_valid[CINE_PAGE_COUNT];

static int cine_gui_accent_shade(int accent, int level)
{
    if (accent == COLOR_ORANGE)
        return level == 0 ? COLOR_ORANGE
             : level == 1 ? COLOR_DARK_ORANGE_MOD
             : COLOR_DARK_GREEN2_MOD;
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
    if (lum < 70)  return cine_gui_accent_shade(accent, 2);
    if (lum < 170) return cine_gui_accent_shade(accent, 1);
    return cine_gui_accent_shade(accent, 0);
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
    struct bmp_file_t * cap, int x0, int y0, int w, int h, int accent, int glow)
{
    if (!cap || !cap->image) return;

    uint8_t * bvram = bmp_vram();
    if (!bvram) return;

    int hi = cine_gui_accent_shade(accent, 0);
    int mid = cine_gui_accent_shade(accent, 1);
    int lo = cine_gui_accent_shade(accent, 2);

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

            int c;
            if (glow && lum > 200)
                c = hi;
            else if (lum > 120)
                c = mid;
            else if (lum > 40)
                c = lo;
            else
                c = COLOR_BLACK;
            dst[xs] = c;
        }
    }
}

void cinema_gui_init(void)
{
    if (gui_ready) return;

    bg_gray = bmp_load(CINE_GUI_BG_FILE, 0);
    cap_left = bmp_load(CINE_GUI_CAP_L_FILE, 0);
    cap_right = bmp_load(CINE_GUI_CAP_R_FILE, 0);

    gui_ready = bg_gray && cap_left && cap_right;
    if (!gui_ready)
    {
        if (bg_gray) { free(bg_gray); bg_gray = 0; }
        if (cap_left) { free(cap_left); cap_left = 0; }
        if (cap_right) { free(cap_right); cap_right = 0; }
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

void cinema_gui_draw_page_bg(cinema_os_page_t page, int x, int y, int w, int h)
{
    if (!gui_ready)
    {
        cine_ui_draw_backdrop(x, y, w, h, cinema_os_page_color(page));
        return;
    }

    cine_gui_build_tinted_bg(page);
    if (tinted_valid[page] && tinted_bg[page])
        cine_gui_blit_cache(x, y, w, h, tinted_bg[page], MIN(bg_gray->width, CINE_GUI_CACHE_W), MIN(bg_gray->height, CINE_GUI_CACHE_H));
    else
        cine_ui_draw_backdrop(x, y, w, h, cinema_os_page_color(page));

    cine_ui_draw_veil_20(x, y, w, h);
}

void cinema_gui_draw_crystal_row(int x, int y, int w, int h, int accent, int scale_pct)
{
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
    int inset = cap_w - 4;
    if (inset < 12) inset = 12;
    if (inset * 2 >= w) inset = w / 4;

    int hi = cine_gui_accent_shade(accent, 0);
    int lo = cine_gui_accent_shade(accent, 2);

    if (gui_ready)
    {
        cine_gui_blit_cap_tinted(cap_left, x, y, inset, h, accent, 1);
        cine_gui_blit_cap_tinted(cap_right, x + w - inset, y, inset, h, accent, 1);
    }
    else
    {
        cine_ui_draw_selection_bar(x, y, w, h, accent, 1);
        return;
    }

    /* Glowing outer rim */
    bmp_draw_rect(hi, x + 1, y + 1, w - 2, h - 2);
    bmp_fill(hi, x, y + h - 3, w, 2);

    /* Dark recessed text compartment */
    int pad = 8;
    bmp_fill(COLOR_BLACK, x + inset - 2, y + pad, w - 2 * inset + 4, h - 2 * pad);
    bmp_fill(lo, x + inset, y + pad + 2, w - 2 * inset, h - 2 * pad - 4);
    bmp_fill(COLOR_BLACK, x + inset + 4, y + pad + 4, w - 2 * inset - 8, h - 2 * pad - 8);
}

void cinema_gui_draw_text_shadow(int font_spec, int x, int y, const char * text, int fg)
{
    bmp_printf(FONT(font_spec, COLOR_GRAY(8), NO_BG_ERASE), x + 2, y + 2, "%s", text);
    bmp_printf(FONT(font_spec, fg, NO_BG_ERASE), x, y, "%s", text);
}

void cinema_gui_draw_nav_bar(
    int y, int bar_h, int active_page,
    const int * page_colors, const char ** labels, const int * icons, int page_count)
{
    bmp_fill(COLOR_BLACK, 0, y, 720, bar_h);
    int tile_w = 720 / page_count;

    for (int i = 0; i < page_count; i++)
    {
        int tx = i * tile_w;
        int c = page_colors[i];
        int sel = (i == active_page);
        int fg = sel ? c : COLOR_GRAY(45);

        bfnt_draw_char(icons[i], tx + 8, y + 10, sel ? COLOR_WHITE : COLOR_GRAY(40), COLOR_BLACK);
        bmp_printf(FONT(sel ? FONT_MED : FONT_SMALL, fg, COLOR_BLACK),
            tx + 32, y + (sel ? 14 : 16), "%s", labels[i]);

        if (sel)
        {
            bmp_fill(c, tx + 2, y + bar_h - 6, tile_w - 4, 5);
            bmp_fill(COLOR_WHITE, tx + 4, y + bar_h - 5, tile_w - 8, 1);
        }
    }
}
