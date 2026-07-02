#ifndef _cinema_os_h_
#define _cinema_os_h_

#include "menu.h"

/* 2026 Cinema OS shell — replaces classic ML menu chrome when enabled */
int cinema_os_enabled(void);
int* cinema_os_enabled_var(void);

int cinema_os_tab_color(struct menu * tab);
void cinema_os_draw_tab_bar(struct menu * first_menu, int y);
void cinema_os_draw_content_background(struct menu * tab);
void cinema_os_get_entry_colors(
    struct menu * tab,
    struct menu_entry * entry,
    struct menu_display_info * info,
    int selected_row,
    int * fg,
    int * bg,
    int * accent
);
int cinema_os_entry_row_height(void);
int cinema_os_tab_bar_height(void);

#endif
