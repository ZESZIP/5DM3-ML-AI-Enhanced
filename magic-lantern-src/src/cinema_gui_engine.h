#ifndef _cinema_gui_engine_h_
#define _cinema_gui_engine_h_

#include "cinema_os.h"

/* Pre-rendered UI assets live on card: ML/CINE_UI/ */
#define CINE_GUI_DIR           "ML/CINE_UI"
#define CINE_GUI_BG_FILE       CINE_GUI_DIR "/low_poly_bg.bmp"
#define CINE_GUI_CAP_L_FILE    CINE_GUI_DIR "/highlight_left_cap.bmp"
#define CINE_GUI_CAP_R_FILE    CINE_GUI_DIR "/highlight_right_cap.bmp"

void cinema_gui_init(void);
int cinema_gui_assets_ready(void);

/* Tinted low-poly mountain background (3:2 canvas, accent per OS page). */
void cinema_gui_draw_page_bg(cinema_os_page_t page, int x, int y, int w, int h);

/* Crystal row highlighter: faceted caps + dark inset center. scale_pct: 100 or 115. */
void cinema_gui_draw_crystal_row(int x, int y, int w, int h, int accent, int scale_pct);

/* White text with subtle drop shadow for legibility on terrain. */
void cinema_gui_draw_text_shadow(int font_spec, int x, int y, const char * text, int fg);

/* Matte nav bar with glowing accent underline on active tab. */
void cinema_gui_draw_nav_bar(
    int y, int bar_h, int active_page,
    const int * page_colors, const char ** labels, const int * icons, int page_count);

#endif
