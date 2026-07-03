/** \file
 * Cinema OS debug mode gate — SETTINGS | SYSTEM | DEBUG MODE.
 */
#include "dryos.h"
#include "config.h"
#include "menu.h"
#include "cinema_debug_mode.h"
#include "cinema_debug.h"

static CONFIG_INT("cinema.debug.mode", cinema_debug_mode, 0);

int cinema_debug_mode_enabled(void) { return cinema_debug_mode; }
int * cinema_debug_mode_var(void) { return &cinema_debug_mode; }

static MENU_UPDATE_FUNC(cinema_debug_mode_update)
{
    MENU_SET_VALUE(cinema_debug_mode ? "ON" : "OFF");
    MENU_SET_HELP("When OFF, CINE debug hooks are disabled for max performance.");
}

static struct menu_entry cinema_system_menu[] = {
    {
        .name = "DEBUG MODE",
        .priv = &cinema_debug_mode,
        .max = 1,
        .choices = CHOICES("OFF", "ON"),
        .update = cinema_debug_mode_update,
        .help = "Global debug flag for Cine AI Enhanced.",
    },
    MENU_EOL,
};

static void cinema_system_menu_init(void * unused)
{
    (void) unused;
    menu_add("Prefs", cinema_system_menu, COUNT(cinema_system_menu) - 1);
}

INIT_FUNC(__FILE__, cinema_system_menu_init);
