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
#include "menu.h"
#include "cinema_boot.h"
#include "cinema_write_engine.h"
#include "cinema_record_apply.h"
#include "cinema_debug.h"
#include "cinema_ui_theme.h"
#include "module.h"
#include "property.h"

static CONFIG_INT("cinema.boot.done", cinema_boot_done, 0);
static CONFIG_INT("cinema.modules.ready", cinema_modules_ready, 0);

static int wizard_active = 0;
static int menu_splash_until = 0;

static char boot_card_label[32] = "No card";
static char boot_hw_label[48] = "Canon 5D Mark III";
static int boot_speed_centi = 0;

extern int menu_redraw_blocked;

int cinema_boot_complete(void) { return cinema_boot_done; }
int cinema_boot_wizard_active(void) { return wizard_active; }

static void boot_draw_frame(int pct, const char * step)
{
    cine_ui_draw_hd_page_bg(COLOR_ORANGE, 0, 480);

    bfnt_draw_char(ICON_ML_MOVIE, 320, 72, COLOR_ORANGE, NO_BG_ERASE);

    bmp_printf(FONT(FONT_CANON, COLOR_WHITE, COLOR_GRAY(14)), 130, 130, "CINE AI ENHANCED");
    bmp_printf(FONT(FONT_MED, COLOR_GRAY(55), COLOR_GRAY(14)), 95, 168,
        "5D Mark III cinema recording OS");

    bmp_printf(FONT(FONT_MED, COLOR_ORANGE, COLOR_GRAY(14)), 40, 210, "%s", boot_hw_label);
    bmp_printf(FONT(FONT_SMALL, COLOR_GRAY(50), COLOR_GRAY(14)), 40, 234, "%s", boot_card_label);

    bmp_printf(FONT(FONT_MED, COLOR_WHITE, COLOR_GRAY(14)), 40, 280, "Setting up your camera...");
    bmp_printf(FONT(FONT_SMALL, COLOR_GRAY(55), COLOR_GRAY(14)), 40, 308, "%s", step);

    int bar_x = 40;
    int bar_y = 340;
    int bar_w = 640;
    int bar_h = 32;
    cine_ui_draw_hd_panel(bar_x, bar_y, bar_w, bar_h, COLOR_ORANGE);
    int fill = (bar_w - 12) * pct / 100;
    if (fill > 0)
        bmp_fill(COLOR_ORANGE, bar_x + 6, bar_y + 10, fill, bar_h - 20);

    bmp_printf(FONT(FONT_MED, COLOR_WHITE, COLOR_GRAY(14)), bar_x + bar_w - 64, bar_y + 8, "%d%%", pct);

    if (pct >= 100)
    {
        bmp_printf(FONT(FONT_MED, COLOR_GREEN1, COLOR_GRAY(14)), 40, 390,
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

static void boot_benchmark_with_progress(void)
{
    boot_draw_frame(38, "Benchmarking card write speed (please wait)...");
    menu_redraw_blocked = 1;

    int t0 = get_ms_clock();
    cinema_write_benchmark_quiet();
    int elapsed = get_ms_clock() - t0;

    boot_speed_centi = cinema_write_speed_centi_mbs();
    if (boot_speed_centi > 0)
    {
        struct card_info * c = get_shooting_card();
        if (c)
            snprintf(boot_card_label, sizeof(boot_card_label), "%s  %d.%02d MB/s",
                c->type, boot_speed_centi / 100, boot_speed_centi % 100);
    }

    int pct = 38 + MIN(22, elapsed / 150);
    boot_draw_frame(pct, "Benchmark complete.");
    msleep(300);
}

static void cinema_request_reboot(const char * reason)
{
    boot_draw_frame(100, reason);
    msleep(2000);
    config_save();
    cine_debug_flush();
    int reboot = 0;
    prop_request_change(PROP_REBOOT, &reboot, 4);
    while (1) msleep(1000);
}

static int cinema_first_boot_enable_modules(void)
{
    wizard_active = 1;
    menu_redraw_blocked = 1;
    cine_debug_init();

    boot_detect_hardware();
    boot_step(8, "First launch: enabling all ML modules...", 500);

    int n = module_enable_all_flagfiles();
    cine_debug_log("enabled %d module .en flags", n);
    module_cine_debug_log(cine_debug_log);

    cinema_modules_ready = 1;
    config_save();

    boot_step(100, "Restarting camera to load modules...", 800);
    cinema_request_reboot("Restarting to activate modules...");
    return 1;
}

static void cinema_boot_run_wizard(void)
{
    wizard_active = 1;
    menu_redraw_blocked = 1;
    cine_debug_init();
    cine_debug_log("wizard start modules_ready=%d", cinema_modules_ready);
    cine_debug_log_modules();

    boot_detect_hardware();
    boot_step(5, "Scanning camera hardware...", 400);

    boot_detect_cards();
    boot_step(15, "Detecting memory cards...", 500);

    boot_step(25, "Running CF/SD integrity test...", 600);
    set_config_var("card.test", 1);

    boot_benchmark_with_progress();

    boot_step(60, "Arming recording engine and performance hacks...", 400);
    cinema_write_arm_hacks();

    boot_step(75, "Calibrating MLV profile for your card...", 400);
    cinema_write_apply_best_profile();

    boot_step(85, "Arming MLV recording engine...", 400);
    if (!cinema_record_apply_full())
    {
        cine_debug_log("MLV arm FAILED during wizard");
        cine_debug_log_mlv_state();
        cine_debug_flush();
        boot_step(85, "MLV arm issue — see ML/LOGS/CINE_DEBUG.LOG", 1200);
    }
    else
    {
        cine_debug_log("MLV arm OK");
        cine_debug_log_mlv_state();
    }

    boot_step(90, "Saving configuration...", 300);
    cinema_boot_done = 1;
    config_save();
    cine_debug_flush();

    boot_step(100, "Setup complete.", 2500);

    wizard_active = 0;
    menu_redraw_blocked = 0;
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
        int wait = 0;
        while (!lv && wait < 120)
        {
            msleep(500);
            wait++;
        }
    }

    if (!cinema_modules_ready)
    {
        canon_gui_disable_front_buffer(0);
        clrscr();
        cinema_first_boot_enable_modules();
        return;
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
    if (wizard_active) return;
    menu_splash_until = get_ms_clock() + 600;
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
    bmp_printf(FONT(FONT_SMALL, COLOR_ORANGE, COLOR_BLACK), 220, 230, "Loading...");
}

int cinema_boot_menu_splash_blocking(void)
{
    return menu_splash_until && get_ms_clock() <= menu_splash_until;
}

TASK_CREATE("cine_boot", cinema_boot_task, 0, 0x1b, 0x3000);
