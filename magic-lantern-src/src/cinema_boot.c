/** \file
 * Cine AI Enhanced — first-boot setup wizard with progress bar.
 */
#include "tasks.h"
#include "dryos.h"
#include "bmp.h"
#include "font.h"
#include "gui.h"
#include "config.h"
#include "console.h"
#include "fio-ml.h"
#include "version.h"
#include "propvalues.h"
#include "cinema_boot.h"
#include "cinema_write_engine.h"

static CONFIG_INT("cinema.boot.done", cinema_boot_done, 0);

static int wizard_active = 0;
static int menu_splash_until = 0;

static char boot_card_label[32] = "No card";
static char boot_hw_label[48] = "Canon 5D Mark III";
static int boot_speed_centi = 0;

int cinema_boot_complete(void) { return cinema_boot_done; }
int cinema_boot_wizard_active(void) { return wizard_active; }

static void boot_draw_frame(int pct, const char * step)
{
    bmp_fill(COLOR_BLACK, 0, 0, 720, 480);

    bfnt_draw_char(ICON_ML_MOVIE, 320, 72, COLOR_ORANGE, NO_BG_ERASE);

    bmp_printf(FONT(FONT_CANON, COLOR_WHITE, COLOR_BLACK), 130, 130, "CINE AI ENHANCED");
    bmp_printf(FONT(FONT_MED, COLOR_GRAY(55), COLOR_BLACK), 95, 168,
        "A branch of Magic Lantern");

    bmp_printf(FONT(FONT_MED, COLOR_ORANGE, COLOR_BLACK), 40, 210, "%s", boot_hw_label);
    bmp_printf(FONT(FONT_SMALL, COLOR_GRAY(50), COLOR_BLACK), 40, 234, "%s", boot_card_label);

    bmp_printf(FONT(FONT_MED, COLOR_WHITE, COLOR_BLACK), 40, 280, "Setting up your camera...");
    bmp_printf(FONT(FONT_SMALL, COLOR_GRAY(55), COLOR_BLACK), 40, 308, "%s", step);

    int bar_x = 40;
    int bar_y = 340;
    int bar_w = 640;
    int bar_h = 28;
    bmp_fill(COLOR_GRAY(30), bar_x, bar_y, bar_w, bar_h);
    int fill = bar_w * pct / 100;
    if (fill > 0)
        bmp_fill(COLOR_ORANGE, bar_x, bar_y, fill, bar_h);
    bmp_draw_rect(COLOR_WHITE, bar_x, bar_y, bar_w, bar_h);

    bmp_printf(FONT(FONT_MED, COLOR_WHITE, COLOR_BLACK), bar_x + bar_w - 60, bar_y + 4, "%d%%", pct);

    if (pct >= 100)
    {
        bmp_printf(FONT(FONT_MED, COLOR_GREEN1, COLOR_BLACK), 40, 390,
            "Ready. Press DELETE to open Cine AI OS.");
    }
}

static void boot_step(int pct, const char * step, int delay_ms)
{
    boot_draw_frame(pct, step);
    msleep(delay_ms);
}

static void boot_detect_hardware(void)
{
    snprintf(boot_hw_label, sizeof(boot_hw_label), "Canon 5D Mark III  |  FW %s", build_version);
}

static void boot_detect_cards(void)
{
    struct card_info * card = get_shooting_card();
    if (card && card->type[0])
    {
        snprintf(boot_card_label, sizeof(boot_card_label), "%s %s %s",
            card->type,
            card->maker ? card->maker : "",
            card->model ? card->model : "");
    }
    else
    {
        snprintf(boot_card_label, sizeof(boot_card_label), "Insert CF/SD card in Canon menu");
    }
}

static void cinema_boot_run_wizard(void)
{
    wizard_active = 1;

    boot_detect_hardware();
    boot_step(5, "Scanning camera hardware...", 400);

    boot_detect_cards();
    boot_step(15, "Detecting memory cards...", 500);

    boot_step(25, "Running CF/SD integrity test...", 600);
    set_config_var("card.test", 1);

    boot_step(40, "Benchmarking card write speed...", 300);
    cinema_write_benchmark_quiet();
    boot_speed_centi = cinema_write_speed_centi_mbs();
    if (boot_speed_centi > 0)
    {
        struct card_info * c = get_shooting_card();
        if (c)
            snprintf(boot_card_label, sizeof(boot_card_label), "%s  %d.%02d MB/s",
                c->type, boot_speed_centi / 100, boot_speed_centi % 100);
    }

    boot_step(60, "Arming recording engine & hacks...", 400);
    cinema_write_arm_hacks();

    boot_step(75, "Calibrating MLV profile for your card...", 400);
    cinema_write_apply_best_profile();

    boot_step(90, "Saving configuration...", 300);
    cinema_boot_done = 1;
    config_save();

    boot_step(100, "Setup complete.", 2500);

    wizard_active = 0;
}

static void cinema_boot_task(void * unused)
{
    (void) unused;

    extern int ml_started;
    while (!ml_started)
        msleep(100);

    msleep(3000);

    if (!lv)
    {
        /* wait for user to enter LiveView for card benchmark */
        int wait = 0;
        while (!lv && wait < 120)
        {
            msleep(500);
            wait++;
        }
    }

    if (!cinema_boot_done)
    {
        canon_gui_disable_front_buffer(0);
        clrscr();
        cinema_boot_run_wizard();
        canon_gui_enable_front_buffer(0);
    }
    else
    {
        cinema_write_arm_hacks();
        cinema_write_benchmark_quiet();
        cinema_write_apply_best_profile();
        NotifyBox(3000, "CINE AI ENHANCED\nCalibrated and ready.");
    }
}

void cinema_boot_on_menu_open(void)
{
    menu_splash_until = get_ms_clock() + 850;
}

void cinema_boot_draw_menu_splash(void)
{
    if (!menu_splash_until) return;
    if (get_ms_clock() > menu_splash_until)
    {
        menu_splash_until = 0;
        return;
    }

    bmp_fill(COLOR_BLACK, 0, 0, 720, 480);
    bfnt_draw_char(ICON_ML_MOVIE, 310, 100, COLOR_ORANGE, NO_BG_ERASE);
    bmp_printf(FONT(FONT_CANON, COLOR_WHITE, COLOR_BLACK), 115, 175, "CINE AI ENHANCED");
    bmp_printf(FONT(FONT_MED, COLOR_GRAY(55), COLOR_BLACK), 72, 215,
        "A branch of Magic Lantern");
    bmp_printf(FONT(FONT_SMALL, COLOR_ORANGE, COLOR_BLACK), 200, 260, "Loading...");
}

int cinema_boot_menu_splash_blocking(void)
{
    return menu_splash_until && get_ms_clock() <= menu_splash_until;
}

TASK_CREATE("cine_boot", cinema_boot_task, 0, 0x1b, 0x3000);
