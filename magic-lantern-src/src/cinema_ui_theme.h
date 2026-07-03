#ifndef _cinema_ui_theme_h_
#define _cinema_ui_theme_h_

#include "menu.h"
#include "cinema_os.h"

/* 5D Mark III Modern OS palette */
#define THEME_COLOR_CINE     COLOR_ORANGE      /* #FF6600 */
#define THEME_COLOR_PHOTO    COLOR_GREEN1      /* #4CD964 */
#define THEME_COLOR_SETTINGS COLOR_YELLOW      /* #FFCC00 */
#define THEME_COLOR_ADDONS   COLOR_LIGHT_BLUE  /* #00A2FF */
#define THEME_COLOR_HACKS    COLOR_MAGENTA     /* #A55EEA */

int cine_ui_theme_color_for_page(cinema_os_page_t page);

void cine_ui_draw_shadow(int x, int y, int w, int h, int depth);
void cine_ui_draw_backdrop(int x, int y, int w, int h, int accent);
void cine_ui_draw_veil_20(int x, int y, int w, int h);
void cine_ui_draw_scrollbar(int x, int y, int h, int total, int visible, int scroll, int accent);
void cine_ui_draw_panel_frame(int bx, int by, int w, int h, int accent, const char * title);
void cine_ui_draw_hd_panel(int x, int y, int w, int h, int accent);
void cine_ui_draw_hd_page_bg(int accent, int y0, int h);
void cine_ui_draw_hd_nav_tab(int x, int y, int w, int h, int accent, int selected, const char * label, int icon);
void cine_ui_draw_selection_bar(int x, int y, int w, int h, int accent, int selected);
void cine_ui_draw_row_card(int x, int y, int w, int h, int accent, int selected);
void cine_ui_draw_submenu_frame(int bx, int by, int w, int h, int accent, const char * title);
int cine_ui_menu_accent(struct menu * menu);

/* 2026 cinema spec — matte nav, flat canvas, emerald highlight, glass panels */
void cine_ui_draw_matte_nav_bar(int y, int bar_h, int active_page, const int * page_colors, const char ** labels, const int * icons, int page_count);
void cine_ui_draw_flat_page_bg(int accent, int y0, int h);
void cine_ui_draw_emerald_highlight(int x, int y, int w, int h);
void cine_ui_draw_glass_panel(int x, int y, int w, int h);
void cine_ui_draw_abbr_icon(int x, int y, const char * abbr, int accent);

#endif
