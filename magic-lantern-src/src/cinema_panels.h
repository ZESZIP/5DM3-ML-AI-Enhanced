#ifndef _cinema_panels_h_
#define _cinema_panels_h_

/* Thick in-window submenus for CINE page rows (no ML help files). */

int cinema_panel_is_open(void);
void cinema_panel_open(int row);
void cinema_panel_close(void);
void cinema_panel_draw(int y0, int body_h);
int cinema_panel_handle_key(unsigned int key);

#endif
