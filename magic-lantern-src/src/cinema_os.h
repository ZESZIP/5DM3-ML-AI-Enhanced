#ifndef _cinema_os_h_
#define _cinema_os_h_

#include "menu.h"

/* 2026 Cinema OS — five-page operational shell */

/* Registry colors (closest ML palette; hex in comments) */
#define CINE_COLOR_SETTINGS  COLOR_YELLOW       /* #FFCC00 */
#define CINE_COLOR_PHOTO     COLOR_GREEN1       /* #4CD964 */
#define CINE_COLOR_CINEMA    COLOR_ORANGE       /* #FF6600 */
#define CINE_COLOR_ADDONS    COLOR_LIGHT_BLUE   /* #00A2FF */
#define CINE_COLOR_HACKS     COLOR_MAGENTA      /* #A55EEA */

#define CINE_NAV_H           56
#define CINE_SUBHEADER_H     40
#define CINE_ROW_H           56
#define CINE_VISIBLE_ROWS    6

typedef enum {
    CINE_PAGE_SETTINGS = 0,
    CINE_PAGE_PHOTO,
    CINE_PAGE_CINEMATIC,
    CINE_PAGE_ADDONS,
    CINE_PAGE_HACKS,
    CINE_PAGE_COUNT
} cinema_os_page_t;

int cinema_os_enabled(void);
int* cinema_os_enabled_var(void);

cinema_os_page_t cinema_os_active_page(void);
void cinema_os_set_page(cinema_os_page_t page);
void cinema_os_page_nav(int delta);

int cinema_os_tab_bar_height(void);
int cinema_os_entry_row_height(void);
int cinema_os_page_color(cinema_os_page_t page);

void cinema_os_draw_nav_bar(int y);
void cinema_os_draw_page_background(cinema_os_page_t page, int y0, int h);
int cinema_os_draw_cinematic_page(int list_y);
int cinema_os_uses_cinematic_canvas(void);

void cinema_os_get_entry_colors(
    struct menu * tab,
    struct menu_entry * entry,
    struct menu_display_info * info,
    int selected_row,
    int * fg,
    int * bg,
    int * accent
);

/* Returns 1 if key consumed. Called from menu.c */
int cinema_os_handle_key(unsigned int key);

/* menu.c: sync ML top-level tab when page changes */
const char * cinema_os_page_menu_name(cinema_os_page_t page);
int cinema_os_page_menu_icon(cinema_os_page_t page);

/* Legacy aliases */
int cinema_os_tab_color(struct menu * tab);
void cinema_os_draw_tab_bar(struct menu * first_menu, int y);
void cinema_os_draw_content_background(struct menu * tab);

#endif
