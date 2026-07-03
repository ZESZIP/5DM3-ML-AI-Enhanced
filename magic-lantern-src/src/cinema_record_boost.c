/** \file
 * Cinema OS recording boost — maximum sustained throughput on 5D Mark III.
 *
 * Arms on record start: powersave override, overlay strip, background task
 * deprioritization, compress-task priority boost. Thermal shutdown only at 100C.
 */
#include "tasks.h"
#include "dryos.h"
#include "config.h"
#include "property.h"
#include "propvalues.h"
#include "bmp.h"
#include "powersave.h"
#include "console.h"
#include "zebra.h"
#include "cinema_record_boost.h"
#include "cinema_thermal.h"
#include "cinema_debug.h"

extern int console_visible;

static CONFIG_INT("cinema.record.boost", cinema_record_boost_cfg, 1);

#define BOOST_MAX_TASKS 48
#define BOOST_PRIO_PENALTY 10
#define BOOST_COMPRESS_PRIO 0x08

static int boost_active = 0;
static int saved_idle_never = -1;
static int saved_global_draw = -1;
static int saved_zebra_rec = -1;
static int saved_console = -1;
static int saved_debug_mode = -1;

static struct task * saved_tasks[BOOST_MAX_TASKS];
static uint32_t saved_prio[BOOST_MAX_TASKS];
static int saved_task_count = 0;

static int boost_task_essential(const char * name)
{
    if (!name) return 1;

    static const char * keep[] = {
        "compress", "raw_rec", "RawRec", "LiveView", "Lv", "lv",
        "GuiMain", "FIO", "Medi", "Storage", "EDMAC", "VSync",
        "Interrupt", "HPTask", "Raws", "raw", "Rec", "Movie",
        "Clock", "Main", "init", "card", "CF", "SD"
    };

    for (int i = 0; i < COUNT(keep); i++)
        if (strstr(name, keep[i])) return 1;

    return 0;
}

static void boost_adjust_tasks(int enter)
{
    struct task * t = first_task;

    if (enter)
    {
        saved_task_count = 0;
        while (t && saved_task_count < BOOST_MAX_TASKS)
        {
            if (t->name)
            {
                if (strstr(t->name, "compress"))
                {
                    saved_tasks[saved_task_count] = t;
                    saved_prio[saved_task_count] = t->run_prio;
                    if (t->run_prio > BOOST_COMPRESS_PRIO)
                        t->run_prio = BOOST_COMPRESS_PRIO;
                    saved_task_count++;
                }
                else if (!boost_task_essential(t->name))
                {
                    saved_tasks[saved_task_count] = t;
                    saved_prio[saved_task_count] = t->run_prio;
                    t->run_prio = MIN(0x1e, t->run_prio + BOOST_PRIO_PENALTY);
                    saved_task_count++;
                }
            }
            t = t->next_task;
        }
        printf("[BOOST] deprioritized %d tasks, compress prio=%d\n",
            saved_task_count, BOOST_COMPRESS_PRIO);
    }
    else
    {
        for (int i = 0; i < saved_task_count; i++)
        {
            if (saved_tasks[i])
                saved_tasks[i]->run_prio = saved_prio[i];
        }
        saved_task_count = 0;
    }
}

void cinema_record_boost_enter(void)
{
    if (!cinema_record_boost_cfg || boost_active) return;
    boost_active = 1;

    saved_idle_never = get_config_var("idle.never.poweroff");
    saved_global_draw = get_global_draw_setting();
    saved_zebra_rec = get_config_var("zebra.rec");
    saved_console = console_visible;
    saved_debug_mode = get_config_var("cinema.debug.mode");

    set_config_var("idle.never.poweroff", 1);
    set_config_var("idle.rec", 0);
    set_config_var("zebra.rec", 0);
    set_config_var("cinema.debug.mode", 0);
    powersave_prohibit();

    if (console_visible)
        console_hide();

    boost_adjust_tasks(1);
    cine_debug_log("record boost ON");
}

void cinema_record_boost_exit(void)
{
    if (!boost_active) return;
    boost_active = 0;

    boost_adjust_tasks(0);

    if (saved_idle_never >= 0)
        set_config_var("idle.never.poweroff", saved_idle_never);
    if (saved_global_draw >= 0)
        set_config_var("global.draw", saved_global_draw);
    if (saved_zebra_rec >= 0)
        set_config_var("zebra.rec", saved_zebra_rec);
    if (saved_debug_mode >= 0)
        set_config_var("cinema.debug.mode", saved_debug_mode);

    saved_idle_never = -1;
    saved_global_draw = -1;
    saved_zebra_rec = -1;
    saved_debug_mode = -1;
    saved_console = -1;

    cine_debug_log("record boost OFF");
}

void cinema_record_boost_tick(void)
{
    if (!boost_active) return;
    powersave_prohibit();
    cinema_thermal_tick();
}

int cinema_record_boost_active(void) { return boost_active; }
