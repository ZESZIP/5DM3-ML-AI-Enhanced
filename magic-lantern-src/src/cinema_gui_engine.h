#ifndef _cinema_gui_engine_h_
#define _cinema_gui_engine_h_

#include "cinema_os.h"

#define CINE_GUI_DIR           "ML/CINE_UI"
#define CINE_GUI_BG_FILE       CINE_GUI_DIR "/low_poly_bg.bmp"
#define CINE_GUI_CAP_L_FILE    CINE_GUI_DIR "/highlight_left_cap.bmp"
#define CINE_GUI_CAP_R_FILE    CINE_GUI_DIR "/highlight_right_cap.bmp"
#define CINE_GUI_CAP_M_FILE    CINE_GUI_DIR "/highlight_mid.bmp"

void cinema_gui_init(void);
int cinema_gui_assets_ready(void);

int cinema_gui_list_top_y(void);

void cinema_gui_draw_full_background(cinema_os_page_t page);
void cinema_gui_draw_status_header(void);
void cinema_gui_draw_status_footer(void);

void cinema_gui_draw_page_bg(cinema_os_page_t page, int x, int y, int w, int h);
void cinema_gui_draw_crystal_row(int x, int y, int w, int h, int accent, int scale_pct);

/* Single list row: label left, value right; crystal bar when selected. */
void cinema_gui_draw_menu_row(
    int x, int y, int w, int row_h,
    int accent, int selected,
    int icon, const char * label, const char * value);

void cinema_gui_draw_text_shadow(int font_spec, int x, int y, const char * text, int fg);
void cinema_gui_draw_text_shadow_r(int font_spec, int x, int y, int w, const char * text, int fg);

void cinema_gui_draw_nav_bar(
    int y, int bar_h, int active_page,
    const int * page_colors, const char ** labels, const int * icons, int page_count);

void cinema_gui_draw_scrollbar(int x, int y, int h, int total, int visible, int scroll, int accent);

#endif
