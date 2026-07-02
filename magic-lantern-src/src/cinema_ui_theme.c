/** \file
 * Cine AI Enhanced — modern menu chrome (shadows, accents, cards).
 */
#include "bmp.h"
#include "font.h"
#include "cinema_ui_theme.h"
#include "cinema_os.h"

void cine_ui_draw_shadow(int x, int y, int w, int h, int depth)
{
    for (int i = depth; i > 0; i--)
        bmp_fill(COLOR_GRAY(8 + i * 3), x + i * 2, y + i * 2, w, h);
}

void cine_ui_draw_selection_bar(int x, int y, int w, int h, int accent, int selected)
{
    if (!selected) return;
    bmp_fill(COLOR_WHITE, x, y, w, h);
    bmp_fill(COLOR_GRAY(42), x + 3, y + 3, w - 6, h - 6);
    bmp_fill(accent, x, y, 10, h);
    bmp_draw_rect(COLOR_WHITE, x, y, w, h);
    bmp_draw_rect(accent, x + 1, y + 1, w - 2, h - 2);
}

void cine_ui_draw_row_card(int x, int y, int w, int h, int accent, int selected)
{
    cine_ui_draw_shadow(x, y, w, h, 3);
    if (selected)
        cine_ui_draw_selection_bar(x, y, w, h, accent, 1);
    else
    {
        bmp_fill(COLOR_GRAY(18), x, y, w, h);
        bmp_fill(accent, x, y, 4, h);
        bmp_draw_rect(COLOR_GRAY(40), x, y, w, h);
    }
}

void cine_ui_draw_submenu_frame(int bx, int by, int w, int h, int accent, const char * title)
{
    cine_ui_draw_shadow(bx, by, w, h, 4);
    bmp_fill(COLOR_BLACK, bx, by, w, h);
    bmp_fill(accent, bx, by, w, 6);
    bmp_fill(COLOR_GRAY(15), bx, by + 6, w, 48);
    bmp_printf(FONT(FONT_CANON, COLOR_WHITE, COLOR_GRAY(15)), bx + 20, by + 14, "%s", title);
    bmp_draw_rect(COLOR_WHITE, bx, by, w, h);
    bmp_draw_rect(accent, bx + 2, by + 2, w - 4, h - 4);
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
