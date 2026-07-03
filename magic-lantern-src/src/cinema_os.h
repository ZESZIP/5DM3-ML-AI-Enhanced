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

/* Feature icons for CINE rows and codec picker */
#define CINE_ICON_MOV         ICON_ML_MOVIE
#define CINE_ICON_CINEPACK    ICON_ML_OVERLAY
#define CINE_ICON_RES         ICON_ML_DISPLAY
#define CINE_ICON_FPS         ICON_ML_MOVIE
#define CINE_ICON_CODEC       ICON_ML_OVERLAY
#define CINE_ICON_GAMMA       ICON_ML_SHOOT
#define CINE_ICON_SHUTTER     ICON_ML_EXPO
#define CINE_ICON_APERTURE    ICON_ML_FOCUS
#define CINE_ICON_ISO         ICON_ML_EXPO
#define CINE_ICON_WB          ICON_ML_SHOOT
#define CINE_ICON_AUDIO       ICON_ML_AUDIO
#define CINE_ICON_SENSOR      ICON_ML_OVERLAY
#define CINE_ICON_BEAST       ICON_ML_MOVIE
#define CINE_ICON_LV          ICON_ML_DISPLAY
#define CINE_ICON_FOCUS       ICON_ML_FOCUS

#define CINE_EMERALD_HI       COLOR_GREEN1

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
int cinema_os_uses_pro_shell(void);
int cinema_os_skin_active(void);
void cinema_os_draw_status_footer(void);
void cinema_os_on_menu_open(void);

/* Legacy name */
#define cinema_os_uses_cine_canvas cinema_os_uses_cinematic_canvas

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
int cinema_os_handle_lr_key(int delta);

/* menu.c: sync ML top-level tab when page changes */
const char * cinema_os_page_menu_name(cinema_os_page_t page);
int cinema_os_page_menu_icon(cinema_os_page_t page);

/* Legacy aliases */
int cinema_os_tab_color(struct menu * tab);
void cinema_os_draw_tab_bar(struct menu * first_menu, int y);
void cinema_os_draw_content_background(struct menu * tab);

#endif
