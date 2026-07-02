/** \file
 * Cinema OS — 2026 full-screen menu shell for 5D3 AI Enhanced builds.
 */
#include "dryos.h"
#include "bmp.h"
#include "menu.h"
#include "config.h"
#include "font.h"

static int cinema_os_menu_visible(struct menu * m)
{
    if (!m || !m->children) return 0;
    for (struct menu_entry * e = m->children; e; e = e->next)
        if (!e->shidden && !e->hidden) return 1;
    return m->selected;
}

static int cinema_os_startswith(const char * str, const char * prefix)
{
    if (!str || !prefix) return 0;
    while (*prefix) { if (*str++ != *prefix++) return 0; }
    return 1;
}

static CONFIG_INT("cinema.os", cinema_os, 1);

int cinema_os_enabled(void) { return cinema_os; }
int* cinema_os_enabled_var(void) { return &cinema_os; }

int cinema_os_tab_color(struct menu * tab)
{
    if (!tab) return COLOR_ORANGE;
    int icon = tab->icon ? tab->icon : tab->name[0];
    switch (icon)
    {
        case ICON_ML_MOVIE:    return COLOR_ORANGE;
        case ICON_ML_SHOOT:    return COLOR_GREEN1;
        case ICON_ML_EXPO:     return COLOR_YELLOW;
        case ICON_ML_FOCUS:    return COLOR_CYAN;
        case ICON_ML_DISPLAY:  return COLOR_LIGHT_BLUE;
        case ICON_ML_OVERLAY:  return COLOR_LIGHT_BLUE;
        case ICON_ML_AUDIO:    return COLOR_MAGENTA;
        case ICON_ML_PREFS:    return COLOR_GRAY(50);
        case ICON_ML_SCRIPT:   return COLOR_DARK_CYAN2_MOD;
        case ICON_ML_DEBUG:    return COLOR_GRAY(40);
        case ICON_ML_MODULES:  return COLOR_GRAY(35);
        case ICON_ML_GAMES:    return COLOR_DARK_GREEN1_MOD;
        case ICON_ML_INFO:     return COLOR_LIGHT_BLUE;
        default:               return COLOR_ORANGE;
    }
}

static int cinema_os_is_movie_tab(struct menu * tab)
{
    return tab && tab->icon == ICON_ML_MOVIE;
}

int cinema_os_tab_bar_height(void) { return 56; }
int cinema_os_entry_row_height(void) { return 40; }

void cinema_os_draw_tab_bar(struct menu * first_menu, int y)
{
    int num = 0;
    for (struct menu * m = first_menu; m; m = m->next)
    {
        if (IS_SUBMENU(m)) continue;
        if (!cinema_os_menu_visible(m) && !m->selected) continue;
        num++;
    }
    if (!num) return;

    int bar_h = cinema_os_tab_bar_height();
    bmp_fill(COLOR_BLACK, 0, y, 720, bar_h);

    int tile_w = 720 / num;
    int x = 0;
    int idx = 0;

    for (struct menu * m = first_menu; m; m = m->next)
    {
        if (IS_SUBMENU(m)) continue;
        if (!cinema_os_menu_visible(m) && !m->selected) continue;

        int color = cinema_os_tab_color(m);
        int sel = m->selected;

        if (sel)
            bmp_fill(color, x, y, tile_w, bar_h);
        else
            bmp_fill(45, x, y, tile_w, bar_h - 1);

        int icon = m->icon ? m->icon : m->name[0];
        int iw = bfnt_char_get_width(icon);
        int fg = sel ? COLOR_WHITE : COLOR_GRAY(55);

        bfnt_draw_char(icon, x + (tile_w - iw) / 2, y + 6, fg, NO_BG_ERASE);

        /* short tab label */
        char lbl[8];
        lbl[0] = 0;
        if (cinema_os_startswith(m->name, "Movie")) STR_APPEND(lbl, "CINE");
        else if (cinema_os_startswith(m->name, "Shoot")) STR_APPEND(lbl, "SHOOT");
        else if (cinema_os_startswith(m->name, "Expo")) STR_APPEND(lbl, "EXPO");
        else if (cinema_os_startswith(m->name, "Focus")) STR_APPEND(lbl, "FOCUS");
        else if (cinema_os_startswith(m->name, "Display")) STR_APPEND(lbl, "DISP");
        else if (cinema_os_startswith(m->name, "Overlay")) STR_APPEND(lbl, "OVR");
        else if (cinema_os_startswith(m->name, "Audio")) STR_APPEND(lbl, "AUDIO");
        else if (cinema_os_startswith(m->name, "Prefs")) STR_APPEND(lbl, "PREFS");
        else if (cinema_os_startswith(m->name, "Debug")) STR_APPEND(lbl, "DBG");
        else if (cinema_os_startswith(m->name, "Help")) STR_APPEND(lbl, "HELP");
        else snprintf(lbl, sizeof(lbl), "%.5s", m->name);

        int lfnt = FONT(FONT_MED, fg, sel ? color : 45);
        int lw = bmp_string_width(lfnt, lbl);
        bmp_printf(lfnt, x + (tile_w - lw) / 2, y + bar_h - font_med.height - 4, "%s", lbl);

        if (sel)
        {
            draw_line(x, y + bar_h - 2, x + tile_w, y + bar_h - 2, COLOR_WHITE);
            draw_line(x, y + bar_h - 1, x + tile_w, y + bar_h - 1, COLOR_WHITE);
        }

        x += tile_w;
        idx++;
    }
}

void cinema_os_draw_content_background(struct menu * tab)
{
    int y0 = cinema_os_tab_bar_height();
    int h = 480 - y0 - 50;

    if (cinema_os_is_movie_tab(tab))
    {
        /* full orange cinema workspace */
        bmp_fill(COLOR_ORANGE, 0, y0, 720, h);
        bmp_printf(FONT(FONT_CANON, COLOR_WHITE, COLOR_ORANGE), 16, y0 + 4, "CINEMA OS");
        bmp_printf(FONT(FONT_MED, COLOR_BLACK, COLOR_ORANGE), 200, y0 + 10, "RAW  MLV  ANAMORPHIC");
    }
    else
    {
        int c = cinema_os_tab_color(tab);
        bmp_fill(COLOR_BLACK, 0, y0, 720, h);
        bmp_fill(c, 0, y0, 8, h);
        bmp_printf(FONT(FONT_MED, c, COLOR_BLACK), 16, y0 + 4, "%s", tab->name);
    }
}

void cinema_os_get_entry_colors(
    struct menu * tab,
    struct menu_entry * entry,
    struct menu_display_info * info,
    int selected_row,
    int * fg,
    int * bg,
    int * accent
)
{
    int tab_c = cinema_os_tab_color(tab);
    int movie = cinema_os_is_movie_tab(tab);
    int sel = entry && entry->selected;
    int warn = info && info->warning_level == MENU_WARN_NOT_WORKING;

    *accent = tab_c;

    if (movie)
    {
        if (sel)
        {
            *bg = COLOR_WHITE;
            *fg = COLOR_ORANGE;
        }
        else if (warn)
        {
            *bg = 45;
            *fg = COLOR_GRAY(45);
        }
        else
        {
            *bg = COLOR_ORANGE;
            *fg = COLOR_WHITE;
        }
    }
    else
    {
        if (sel)
        {
            *bg = tab_c;
            *fg = COLOR_WHITE;
        }
        else if (warn)
        {
            *bg = 45;
            *fg = COLOR_GRAY(45);
        }
        else
        {
            *bg = COLOR_BLACK;
            *fg = COLOR_GRAY(60);
        }
    }

    (void)selected_row;
}
