#ifndef _cinema_boot_h_
#define _cinema_boot_h_

/* Cine AI Enhanced — first-boot wizard and setup */

int cinema_boot_complete(void);
int cinema_boot_wizard_active(void);
void cinema_boot_on_menu_open(void);
void cinema_boot_draw_menu_splash(void);
int cinema_boot_menu_splash_blocking(void);

#endif
