/** \file
 * Cinema OS GUI engine — card BMP assets, emerald highlighter, modern menu bypass.
 */
#include "dryos.h"
#include "bmp.h"
#include "font.h"
#include "menu.h"
#include "battery.h"
#include "fio-ml.h"
#include "cinema_gui_engine.h"
#include "cinema_ui_theme.h"
#include "cinema_os.h"

#define CINE_GUI_CACHE_W  720
#define CINE_GUI_CACHE_H  480

/* ---------------------------------------------------------
 * 1. LOAD ASSETS FROM CF/SD CARD ON BOOT
 * --------------------------------------------------------- */
struct bmp_file_t * low_poly_bg = 0;
struct bmp_file_t * highlight_left_cap = 0;
struct bmp_file_t * highlight_right_cap = 0;

static struct bmp_file_t * cap_mid = 0;
static int gui_ready = 0;

static uint8_t * tinted_bg[CINE_PAGE_COUNT];
static int tinted_valid[CINE_PAGE_COUNT];

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
    if (!low_poly_bg || !low_poly_bg->image) return;
    if (tinted_valid[page] && tinted_bg[page]) return;

    int accent = cine_ui_theme_color_for_page(page);
    int bw = low_poly_bg->width;
    int bh = low_poly_bg->height;
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
            int lum = low_poly_bg->image[src_x + src_y * bw];
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

static void cine_gui_draw_crystal_procedural(int x, int y, int w, int h, int active_color)
{
    int hi = active_color;
    int mid = cine_gui_accent_shade(active_color, 2);
    int point = 24;

    bmp_fill(mid, x + point, y, w - 2 * point, h);
    for (int i = 0; i < point; i++)
    {
        int slice_w = MAX(1, (point - i) * 2);
        bmp_fill(hi, x + i, y + i * h / (2 * point), slice_w, h - i * h / point);
        bmp_fill(hi, x + w - i - slice_w, y + i * h / (2 * point), slice_w, h - i * h / point);
    }
    bmp_fill(COLOR_BLACK, x + point + 8, y + 10, w - 2 * point - 16, h - 20);
}

/* ---------------------------------------------------------
 * 2. THE 3D EMERALD HIGHLIGHTER ROUTINE
 * --------------------------------------------------------- */
void draw_emerald_highlighter(int x, int y, int width, int height, int active_color)
{
    if (!highlight_left_cap || !highlight_right_cap)
    {
        cine_gui_draw_crystal_procedural(x, y, width, height, active_color);
        return;
    }

    int lcap_w = highlight_left_cap->width;
    int rcap_w = highlight_right_cap->width;

    /* A. Left 3D crystal cap (scaled to row height) */
    bmp_draw_scaled_ex(highlight_left_cap, x, y, lcap_w, height, 0);

    /* B. Center darkened glass compartment */
    int center_x = x + lcap_w;
    int center_width = width - lcap_w - rcap_w;
    if (center_width < 8) center_width = 8;

    bmp_fill(COLOR_BLACK, center_x, y + 4, center_width, height - 8);
    for (int yy = y + 6; yy < y + height - 6; yy += 3)
        bmp_fill(COLOR_GRAY(8), center_x + 2, yy, center_width - 4, 1);

    bmp_fill(active_color, center_x, y, center_width, 3);
    bmp_fill(active_color, center_x, y + height - 3, center_width, 3);

    /* C. Right 3D crystal cap */
    bmp_draw_scaled_ex(highlight_right_cap, center_x + center_width, y, rcap_w, height, 0);
}

static struct menu_entry * cine_gui_visible_at(struct menu * m, int target)
{
    int vi = 0;
    for (struct menu_entry * e = m->children; e; e = e->next)
    {
        if (!menu_cinema_entry_visible(e))
            continue;
        if (vi == target)
            return e;
        vi++;
    }
    return 0;
}

static int cine_gui_visible_count(struct menu * m)
{
    int n = 0;
    for (struct menu_entry * e = m->children; e; e = e->next)
        if (menu_cinema_entry_visible(e))
            n++;
    return n;
}

/* ---------------------------------------------------------
 * 3. THE MAIN RENDER OVERRIDE (BYPASSES MENU.C ROW DRAW)
 * --------------------------------------------------------- */
void cinema_draw_modern_menu(
    struct menu * menu_to_draw,
    int scroll_index,
    int selected_index,
    int active_mode_color)
{
    if (!menu_to_draw) return;

    int start_y = cinema_gui_list_top_y();
    int standard_row_height = CINE_ROW_H;
    int highlight_scale_offset = standard_row_height * 15 / 100;
    int max_rows = CINE_VISIBLE_ROWS;
    int total = cine_gui_visible_count(menu_to_draw);
    int list_y = start_y;
    int extra_push = 0;

    for (int i = 0; i < max_rows; i++)
    {
        int row_idx = scroll_index + i;
        if (row_idx >= total) break;

        struct menu_entry * entry = cine_gui_visible_at(menu_to_draw, row_idx);
        if (!entry) continue;

        struct menu_display_info info;
        menu_cinema_fill_entry_info(menu_to_draw, entry, &info);
        if (info.custom_drawing == CUSTOM_DRAW_THIS_MENU)
            return;

        int current_y = list_y + extra_push;
        int sel = (row_idx == selected_index);

        if (sel)
        {
            current_y += highlight_scale_offset / 2;
            int bar_h = standard_row_height + highlight_scale_offset;

            draw_emerald_highlighter(30, current_y, 660, bar_h, active_mode_color);

            bmp_printf(
                FONT(FONT_CANON, COLOR_WHITE, NO_BG_ERASE),
                60, current_y + 12, "%s", info.name);
            if (info.value[0])
                bmp_printf(
                    FONT(FONT_MED, COLOR_WHITE, NO_BG_ERASE) | FONT_ALIGN_RIGHT,
                    690, current_y + 14, "%s", info.value);

            extra_push += highlight_scale_offset;
        }
        else
        {
            bmp_printf(
                FONT(FONT_LARGE, COLOR_WHITE, NO_BG_ERASE),
                60, current_y + 10, "%s", info.name);
            if (info.value[0])
                bmp_printf(
                    FONT(FONT_MED, COLOR_WHITE, NO_BG_ERASE) | FONT_ALIGN_RIGHT,
                    690, current_y + 12, "%s", info.value);
        }

        list_y += standard_row_height;
    }

    cinema_gui_draw_scrollbar(712, start_y, max_rows * standard_row_height + extra_push,
        total, max_rows, scroll_index, active_mode_color);
}

int cinema_gui_list_top_y(void)
{
    return CINE_HDR_H + CINE_NAV_H + 12;
}

void cinema_gui_init(void)
{
    if (gui_ready) return;

    low_poly_bg = bmp_load(CINE_GUI_BG_FILE, 0);
    highlight_left_cap = bmp_load(CINE_GUI_CAP_L_FILE, 0);
    highlight_right_cap = bmp_load(CINE_GUI_CAP_R_FILE, 0);
    cap_mid = bmp_load(CINE_GUI_CAP_M_FILE, 0);

    gui_ready = low_poly_bg && highlight_left_cap && highlight_right_cap;
    if (!gui_ready)
    {
        if (low_poly_bg) { free(low_poly_bg); low_poly_bg = 0; }
        if (highlight_left_cap) { free(highlight_left_cap); highlight_left_cap = 0; }
        if (highlight_right_cap) { free(highlight_right_cap); highlight_right_cap = 0; }
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
    if (low_poly_bg)
    {
        cinema_os_page_t p = page;
        cine_gui_build_tinted_bg(p);
        if (tinted_valid[p] && tinted_bg[p])
            cine_gui_blit_cache(0, 0, 720, 480, tinted_bg[p],
                MIN(low_poly_bg->width, CINE_GUI_CACHE_W),
                MIN(low_poly_bg->height, CINE_GUI_CACHE_H));
        else
            bmp_draw_scaled_ex(low_poly_bg, 0, 0, 720, 480, 0);
    }
    else
        cine_ui_draw_backdrop(0, 0, 720, 480, cine_ui_theme_color_for_page(page));
}

void cinema_gui_draw_page_bg(cinema_os_page_t page, int x, int y, int w, int h)
{
    if (!gui_ready)
    {
        cine_ui_draw_backdrop(x, y, w, h, cine_ui_theme_color_for_page(page));
        return;
    }

    cine_gui_build_tinted_bg(page);
    if (tinted_valid[page] && tinted_bg[page])
        cine_gui_blit_cache(x, y, w, h, tinted_bg[page],
            MIN(low_poly_bg->width, CINE_GUI_CACHE_W),
            MIN(low_poly_bg->height, CINE_GUI_CACHE_H));
    else if (low_poly_bg)
        bmp_draw_scaled_ex(low_poly_bg, x, y, w, h, 0);
    else
        cine_ui_draw_backdrop(x, y, w, h, cine_ui_theme_color_for_page(page));
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
    if (scale_pct != 100)
    {
        int nw = w * scale_pct / 100;
        int nh = h * scale_pct / 100;
        x -= (nw - w) / 2;
        y -= (nh - h) / 2;
        w = nw;
        h = nh;
    }
    draw_emerald_highlighter(x, y, w, h, accent);
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
        draw_emerald_highlighter(x, ny, w, nh, accent);

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
