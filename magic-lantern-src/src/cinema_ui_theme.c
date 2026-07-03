/** \file
 * Cine AI Enhanced — low-poly accent backgrounds, layered under menu chrome.
 *
 * Draw order (back → front):
 *   1. Black base + accent low-poly chamfer blocks
 *   2. ~20% black dither veil
 *   3. Menu rows / text / controls (drawn by callers after backdrop)
 */
#include "bmp.h"
#include "font.h"
#include "cinema_ui_theme.h"
#include "cinema_os.h"

#define CINE_CHAMFER 6

static int cine_ui_accent_shade(int accent, int level)
{
    /* level 0 = highlight, 1 = base, 2 = shadow */
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

static void cine_ui_fill_chamfer_block(
    int x, int y, int w, int h,
    int fill_c, int edge_c, int chamfer)
{
    if (w < 12 || h < 12) return;
    bmp_fill(fill_c, x, y, w, h);
    bmp_draw_rect_chamfer(edge_c, x, y, w, h, chamfer, 1);
}

static void cine_ui_draw_lowpoly_blocks(int x, int y, int w, int h, int accent)
{
    int s0 = cine_ui_accent_shade(accent, 0);
    int s1 = cine_ui_accent_shade(accent, 1);
    int s2 = cine_ui_accent_shade(accent, 2);

    bmp_fill(COLOR_BLACK, x, y, w, h);

    cine_ui_fill_chamfer_block(x, y, w * 56 / 100, h * 38 / 100, s2, s1, CINE_CHAMFER);
    cine_ui_fill_chamfer_block(x + w * 44 / 100, y, w - w * 44 / 100, h * 30 / 100, s1, s0, CINE_CHAMFER);
    cine_ui_fill_chamfer_block(x, y + h * 34 / 100, w * 50 / 100, h - h * 34 / 100, s1, s2, CINE_CHAMFER);
    cine_ui_fill_chamfer_block(x + w * 48 / 100, y + h * 26 / 100, w - w * 48 / 100, h - h * 26 / 100, s0, s1, CINE_CHAMFER);
    cine_ui_fill_chamfer_block(x + w * 18 / 100, y + h * 12 / 100, w * 34 / 100, h * 42 / 100, s2, s0, CINE_CHAMFER);
    cine_ui_fill_chamfer_block(x + w * 62 / 100, y + h * 48 / 100, w * 30 / 100, h * 38 / 100, s1, s0, CINE_CHAMFER);
    cine_ui_fill_chamfer_block(x + w * 6 / 100, y + h * 58 / 100, w * 28 / 100, h * 34 / 100, s0, s2, CINE_CHAMFER);
}

void cine_ui_draw_veil_20(int x, int y, int w, int h)
{
    if (w <= 0 || h <= 0) return;
    /* Fast ~20% darkening: horizontal scanlines (vs per-pixel dither) */
    for (int yy = y + 3; yy < y + h; yy += 5)
        bmp_fill(COLOR_BLACK, x, yy, w, 1);
}

void cine_ui_draw_scrollbar(int x, int y, int h, int total, int visible, int scroll, int accent)
{
    if (total <= visible) return;
    int track_h = h - 8;
    int thumb_h = MAX(20, track_h * visible / total);
    int max_scroll = total - visible;
    int thumb_y = y + 4 + (track_h - thumb_h) * scroll / MAX(1, max_scroll);

    bmp_fill(accent, x, y + 4, 4, track_h);
    bmp_fill(COLOR_WHITE, x, thumb_y, 4, thumb_h);
}

void cine_ui_draw_panel_frame(int bx, int by, int w, int h, int accent, const char * title)
{
    int s0 = cine_ui_accent_shade(accent, 0);
    int s1 = cine_ui_accent_shade(accent, 1);

    cine_ui_draw_shadow(bx, by, w, h, 4);
    bmp_fill(COLOR_BLACK, bx, by, w, h);
    cine_ui_fill_chamfer_block(bx + 2, by + 2, w - 4, 46, s0, s1, CINE_CHAMFER);
    bmp_printf(FONT(FONT_CANON, COLOR_WHITE, s0), bx + 24, by + 16, "%s", title);
    bmp_draw_rect_chamfer(s0, bx, by, w, h, CINE_CHAMFER, 1);
}

void cine_ui_draw_backdrop(int x, int y, int w, int h, int accent)
{
    cine_ui_draw_lowpoly_blocks(x, y, w, h, accent);
    cine_ui_draw_veil_20(x, y, w, h);
}

#include "cinema_os.h"

int cine_ui_theme_color_for_page(cinema_os_page_t page)
{
    switch (page)
    {
        case CINE_PAGE_SETTINGS: return THEME_COLOR_SETTINGS;
        case CINE_PAGE_PHOTO:    return THEME_COLOR_PHOTO;
        case CINE_PAGE_CINEMATIC:return THEME_COLOR_CINE;
        case CINE_PAGE_ADDONS:   return THEME_COLOR_ADDONS;
        case CINE_PAGE_HACKS:    return THEME_COLOR_HACKS;
        default:                 return THEME_COLOR_CINE;
    }
}

void cine_ui_draw_shadow(int x, int y, int w, int h, int depth)
{
    for (int i = depth; i > 0; i--)
        bmp_fill(COLOR_BLACK, x + i * 2, y + i * 2, w, h);
}

void cine_ui_draw_hd_panel(int x, int y, int w, int h, int accent)
{
    int s0 = cine_ui_accent_shade(accent, 0);
    int s1 = cine_ui_accent_shade(accent, 1);
    int s2 = cine_ui_accent_shade(accent, 2);

    cine_ui_draw_shadow(x, y, w, h, 4);
    cine_ui_fill_chamfer_block(x, y, w, h, s1, s0, CINE_CHAMFER);
    bmp_fill(s2, x + 4, y + 4, 10, h - 8);
    bmp_fill(s0, x + w - 12, y + 6, 6, h - 12);
}

void cine_ui_draw_hd_page_bg(int accent, int y0, int h)
{
    cine_ui_draw_backdrop(0, y0, 720, h, accent);
    bmp_fill(cine_ui_accent_shade(accent, 0), 14, y0 + 10, 692, 4);
}

void cine_ui_draw_hd_nav_tab(int x, int y, int w, int h, int accent, int selected, const char * label, int icon)
{
    int s0 = cine_ui_accent_shade(accent, 0);
    int s1 = cine_ui_accent_shade(accent, 1);
    int s2 = cine_ui_accent_shade(accent, 2);

    if (selected)
    {
        cine_ui_draw_shadow(x + 2, y + 2, w - 4, h - 6, 3);
        cine_ui_fill_chamfer_block(x + 2, y + 2, w - 4, h - 6, s0, s1, CINE_CHAMFER);
        bfnt_draw_char(icon, x + 12, y + 10, COLOR_WHITE, NO_BG_ERASE);
        bmp_printf(FONT(FONT_MED, COLOR_WHITE, s0), x + 40, y + 14, "%s", label);
    }
    else
    {
        cine_ui_fill_chamfer_block(x + 4, y + 8, w - 8, h - 12, s2, s1, CINE_CHAMFER);
        bmp_fill(s1, x + 4, y + 8, 6, h - 12);
        bfnt_draw_char(icon, x + 12, y + 12, s0, NO_BG_ERASE);
        bmp_printf(FONT(FONT_SMALL, s0, s2), x + 38, y + 16, "%s", label);
    }
}

void cine_ui_draw_selection_bar(int x, int y, int w, int h, int accent, int selected)
{
    if (!selected) return;
    int s0 = cine_ui_accent_shade(accent, 0);
    int s1 = cine_ui_accent_shade(accent, 1);

    cine_ui_draw_shadow(x, y, w, h, 2);
    cine_ui_fill_chamfer_block(x, y, w, h, s0, s1, CINE_CHAMFER);
    bmp_fill(COLOR_BLACK, x + 6, y + 6, w - 12, h - 12);
    bmp_fill(s0, x, y, 12, h);
    bmp_draw_rect_chamfer(COLOR_WHITE, x, y, w, h, CINE_CHAMFER, 0);
}

void cine_ui_draw_row_card(int x, int y, int w, int h, int accent, int selected)
{
    if (selected)
    {
        cine_ui_draw_selection_bar(x, y, w, h, accent, 1);
        return;
    }

    int s0 = cine_ui_accent_shade(accent, 0);
    int s1 = cine_ui_accent_shade(accent, 1);
    int s2 = cine_ui_accent_shade(accent, 2);

    cine_ui_draw_shadow(x, y, w, h, 3);
    cine_ui_fill_chamfer_block(x, y, w, h, s1, s0, CINE_CHAMFER);
    bmp_fill(s2, x, y, 10, h);
    bmp_fill(s0, x + w - 8, y + 4, 6, h - 8);
}

void cine_ui_draw_submenu_frame(int bx, int by, int w, int h, int accent, const char * title)
{
    int s0 = cine_ui_accent_shade(accent, 0);
    int s1 = cine_ui_accent_shade(accent, 1);

    cine_ui_draw_backdrop(bx, by, w, h, accent);
    cine_ui_fill_chamfer_block(bx + 2, by + 2, w - 4, 46, s0, s1, CINE_CHAMFER);
    bmp_printf(FONT(FONT_CANON, COLOR_WHITE, s0), bx + 24, by + 16, "%s", title);
    bmp_draw_rect_chamfer(s0, bx, by, w, h, CINE_CHAMFER, 1);
}

int cine_ui_menu_accent(struct menu * menu)
{
    if (!menu) return CINE_COLOR_CINEMA;
    if (!cinema_os_enabled()) return CINE_COLOR_CINEMA;
    switch (menu->icon)
    {
        case ICON_ML_MOVIE:    return CINE_COLOR_CINEMA;
        case ICON_ML_SHOOT:    return CINE_COLOR_PHOTO;
        case ICON_ML_EXPO:
        case ICON_ML_PREFS:    return CINE_COLOR_SETTINGS;
        case ICON_ML_MODULES:
        case ICON_ML_SCRIPT:   return CINE_COLOR_ADDONS;
        case ICON_ML_DEBUG:    return CINE_COLOR_HACKS;
        default:               return CINE_COLOR_CINEMA;
    }
}

#define CINE_EMERALD_HI   COLOR_GREEN1
#define CINE_EMERALD_MID  COLOR_DARK_GREEN1_MOD
#define CINE_EMERALD_LO   COLOR_DARK_GREEN2_MOD

void cine_ui_draw_matte_nav_bar(
    int y, int bar_h, int active_page,
    const int * page_colors, const char ** labels, const int * icons, int page_count)
{
    bmp_fill(COLOR_BLACK, 0, y, 720, bar_h);
    int tile_w = 720 / page_count;

    for (int i = 0; i < page_count; i++)
    {
        int x = i * tile_w;
        int c = page_colors[i];
        int sel = (i == active_page);
        int fg = sel ? c : COLOR_GRAY(40);
        int icon_fg = sel ? COLOR_WHITE : COLOR_GRAY(35);

        bfnt_draw_char(icons[i], x + 10, y + 10, icon_fg, COLOR_BLACK);
        bmp_printf(FONT(sel ? FONT_MED : FONT_SMALL, fg, COLOR_BLACK),
            x + 36, y + (sel ? 14 : 16), "%s", labels[i]);

        if (sel)
            bmp_fill(c, x + 4, y + bar_h - 5, tile_w - 8, 4);
    }
}

void cine_ui_draw_flat_page_bg(int accent, int y0, int h)
{
    bmp_fill(accent, 0, y0, 720, h);
}

void cine_ui_draw_emerald_highlight(int x, int y, int w, int h)
{
    cine_ui_draw_shadow(x - 2, y - 4, w + 4, h + 8, 5);
    cine_ui_fill_chamfer_block(x - 2, y - 4, w + 4, h + 8, CINE_EMERALD_MID, CINE_EMERALD_HI, 8);
    cine_ui_fill_chamfer_block(x + 4, y + 2, w - 8, h - 4, CINE_EMERALD_LO, CINE_EMERALD_MID, 6);
    bmp_fill(COLOR_BLACK, x + 10, y + 6, w - 20, h - 12);
    bmp_draw_rect_chamfer(CINE_EMERALD_HI, x - 2, y - 4, w + 4, h + 8, 8, 0);
    bmp_fill(CINE_EMERALD_HI, x, y - 4, w, 3);
}

void cine_ui_draw_glass_panel(int x, int y, int w, int h)
{
    cine_ui_draw_shadow(x, y, w, h, 4);
    for (int yy = y; yy < y + h; yy += 2)
        bmp_fill(COLOR_GRAY(50), x, yy, w, 1);
    bmp_fill(COLOR_WHITE, x, y, w, 2);
    bmp_fill(COLOR_WHITE, x, y, 2, h);
    bmp_draw_rect(COLOR_WHITE, x, y, w, h);
    bmp_draw_rect(COLOR_GRAY(60), x + 2, y + 2, w - 4, h - 4);
}

void cine_ui_draw_abbr_icon(int x, int y, const char * abbr, int accent)
{
    bmp_draw_rect(COLOR_WHITE, x, y, 44, 36);
    bmp_fill(COLOR_BLACK, x + 1, y + 1, 42, 34);
    bmp_printf(FONT(FONT_SMALL, COLOR_WHITE, COLOR_BLACK), x + 4, y + 12, "%s", abbr);
    (void) accent;
}
