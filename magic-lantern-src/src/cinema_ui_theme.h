#ifndef _cinema_ui_theme_h_
#define _cinema_ui_theme_h_

#include "menu.h"

void cine_ui_draw_shadow(int x, int y, int w, int h, int depth);
void cine_ui_draw_backdrop(int x, int y, int w, int h, int accent);
void cine_ui_draw_veil_20(int x, int y, int w, int h);
void cine_ui_draw_hd_panel(int x, int y, int w, int h, int accent);
void cine_ui_draw_hd_page_bg(int accent, int y0, int h);
void cine_ui_draw_hd_nav_tab(int x, int y, int w, int h, int accent, int selected, const char * label, int icon);
void cine_ui_draw_selection_bar(int x, int y, int w, int h, int accent, int selected);
void cine_ui_draw_row_card(int x, int y, int w, int h, int accent, int selected);
void cine_ui_draw_submenu_frame(int bx, int by, int w, int h, int accent, const char * title);
int cine_ui_menu_accent(struct menu * menu);

#endif
