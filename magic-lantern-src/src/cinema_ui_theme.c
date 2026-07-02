/** \file
 * Cine AI Enhanced — modern menu chrome (shadows, HD panels, accents).
 */
#include "bmp.h"
#include "font.h"
#include "cinema_ui_theme.h"
#include "cinema_os.h"

void cine_ui_draw_shadow(int x, int y, int w, int h, int depth)
{
    for (int i = depth; i > 0; i--)
        bmp_fill(COLOR_GRAY(6 + i * 4), x + i * 2, y + i * 2, w, h);
}

static void cine_ui_draw_mesh(int x, int y, int w, int h, int accent)
{
    for (int i = 0; i < w + h; i += 28)
    {
        draw_line(x, y + h - i, x + i, y, COLOR_GRAY(14));
        if (i % 56 == 0)
            draw_line(x + w - i, y, x + w, y + i, accent);
    }
}

void cine_ui_draw_hd_panel(int x, int y, int w, int h, int accent)
{
    cine_ui_draw_shadow(x, y, w, h, 5);
    bmp_fill(COLOR_GRAY(8), x, y, w, h);
    cine_ui_draw_mesh(x, y, w, h, accent);
    bmp_fill(accent, x, y, w, 5);
    bmp_fill(COLOR_GRAY(14), x + 3, y + 8, w - 6, h - 11);
    bmp_draw_rect_chamfer(accent, x, y, w, h, 6, 0);
    bmp_draw_rect_chamfer(COLOR_GRAY(35), x + 2, y + 2, w - 4, h - 4, 5, 1);
    bmp_draw_rect_chamfer(COLOR_GRAY(18), x + 4, y + 7, w - 8, h - 12, 4, 1);
}

void cine_ui_draw_hd_page_bg(int accent, int y0, int h)
{
    bmp_fill(COLOR_GRAY(6), 0, y0, 720, h);
    cine_ui_draw_mesh(0, y0, 720, h, accent);

    cine_ui_draw_hd_panel(6, y0 + 2, 708, h - 4, accent);

    bmp_fill(accent, 12, y0 + 10, 696, 3);
    for (int g = 0; g < 4; g++)
        bmp_fill(COLOR_GRAY(10 + g * 2), 12, y0 + 16 + g * 3, 696, 2);
}

void cine_ui_draw_hd_nav_tab(int x, int y, int w, int h, int accent, int selected, const char * label, int icon)
{
    if (selected)
    {
        cine_ui_draw_shadow(x + 2, y + 2, w - 4, h - 6, 3);
        bmp_fill(COLOR_GRAY(16), x + 2, y + 2, w - 4, h - 6);
        bmp_fill(accent, x + 2, y + 2, w - 4, 5);
        bmp_draw_rect_chamfer(accent, x + 2, y + 2, w - 4, h - 6, 5, 0);
        bfnt_draw_char(icon, x + 12, y + 10, accent, NO_BG_ERASE);
        bmp_printf(FONT(FONT_MED, COLOR_WHITE, COLOR_GRAY(16)), x + 40, y + 14, "%s", label);
        bmp_fill(accent, x + 8, y + h - 8, w - 16, 3);
    }
    else
    {
        bmp_fill(COLOR_GRAY(10), x + 4, y + 8, w - 8, h - 12);
        bmp_fill(accent, x + 4, y + 8, 4, h - 12);
        bfnt_draw_char(icon, x + 12, y + 12, COLOR_GRAY(45), NO_BG_ERASE);
        bmp_printf(FONT(FONT_SMALL, COLOR_GRAY(50), COLOR_GRAY(10)), x + 38, y + 16, "%s", label);
    }
}

void cine_ui_draw_selection_bar(int x, int y, int w, int h, int accent, int selected)
{
    if (!selected) return;
    cine_ui_draw_shadow(x, y, w, h, 2);
    bmp_fill(COLOR_WHITE, x, y, w, h);
    bmp_fill(COLOR_GRAY(42), x + 4, y + 4, w - 8, h - 8);
    bmp_fill(accent, x, y, 12, h);
    bmp_draw_rect_chamfer(accent, x, y, w, h, 4, 0);
    bmp_draw_rect_chamfer(COLOR_WHITE, x + 1, y + 1, w - 2, h - 2, 3, 1);
}

void cine_ui_draw_row_card(int x, int y, int w, int h, int accent, int selected)
{
    if (selected)
    {
        cine_ui_draw_selection_bar(x, y, w, h, accent, 1);
        return;
    }

    cine_ui_draw_shadow(x, y, w, h, 3);
    bmp_fill(COLOR_GRAY(12), x, y, w, h);
    bmp_fill(accent, x, y, 6, h);
    bmp_draw_rect_chamfer(COLOR_GRAY(30), x, y, w, h, 5, 0);
    bmp_draw_rect_chamfer(COLOR_GRAY(16), x + 2, y + 2, w - 4, h - 4, 4, 1);
}

void cine_ui_draw_submenu_frame(int bx, int by, int w, int h, int accent, const char * title)
{
    cine_ui_draw_hd_panel(bx, by, w, h, accent);
    bmp_fill(accent, bx, by + 5, w, 44);
    bmp_fill(COLOR_GRAY(12), bx + 8, by + 52, w - 16, h - 60);
    bmp_printf(FONT(FONT_CANON, COLOR_WHITE, accent), bx + 24, by + 16, "%s", title);
    bmp_draw_rect_chamfer(COLOR_WHITE, bx + 4, by + 4, w - 8, h - 8, 5, 1);
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
