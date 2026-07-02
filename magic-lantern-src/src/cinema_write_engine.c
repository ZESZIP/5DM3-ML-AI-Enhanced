/** \file
 * Cinema OS write engine — live CF/SD benchmark, auto profile, pre-armed hacks.
 */
#include "tasks.h"
#include "dryos.h"
#include "bmp.h"
#include "config.h"
#include "propvalues.h"
#include "fio-ml.h"
#include "fps.h"
#include "console.h"

static CONFIG_INT("cinema.write.engine", cinema_write_engine_on, 1);
static CONFIG_INT("cinema.write.autoprofile", cinema_write_autoprofile, 1);

static int write_speed_centi = 0;
static int write_engine_ready = 0;
static int write_profile_tier = 0;
static char write_profile_name[32] = "MANUAL";
static int last_live_probe_ms = 0;

#define CINE_BENCH_FILE "DCIM/.cine_bench"

static int cinema_write_measure(int bufsize, int duration_ms)
{
    FIO_RemoveFile(CINE_BENCH_FILE);
    msleep(200);

    FILE * f = FIO_CreateFile(CINE_BENCH_FILE);
    if (!f) return 0;

    int t0 = get_ms_clock();
    int total = 0;
    int loops = 0;

    while (get_ms_clock() - t0 < duration_ms && loops < 200)
    {
        int r = FIO_WriteFile(f, (const void *) 0x50000000, bufsize);
        if (r <= 0) break;
        total += r;
        loops++;
    }

    FIO_CloseFile(f);
    FIO_RemoveFile(CINE_BENCH_FILE);

    int t1 = get_ms_clock();
    if (t1 <= t0 || total <= 0) return 0;

    /* centi-MB/s (same unit as raw.write.speed) */
    return (int) ((long long) total * 100 / 1024 / (t1 - t0));
}

void cinema_write_benchmark_quiet(void)
{
    struct card_info * card = get_shooting_card();
    if (!card || card->free_space_raw < 50) return;

    int speed = cinema_write_measure(16 * 1024 * 1024, 3500);
    if (speed <= 0)
        speed = cinema_write_measure(4 * 1024 * 1024, 2500);

    if (speed > 0)
    {
        write_speed_centi = speed;
        set_config_var("raw.write.speed", speed);
        write_engine_ready = 1;
        DryosDebugMsg(0, 15, "Cinema write engine: %d.%02d MB/s on %s",
            speed / 100, speed % 100, card->type);
    }
}

void cinema_write_arm_hacks(void)
{
    set_config_var("cinema.os", 1);
    set_config_var("cinema.governor", 1);
    set_config_var("cinema.write.engine", 1);
    set_config_var("idle.never.poweroff", 1);
    set_config_var("card.test", 1);
    set_config_var("raw.small.hacks", 1);
    set_config_var("raw.use.srm.memory", 1);
    set_config_var("raw.preview", 2);           /* ML preview path */
    set_config_var("raw.sync_beep", 1);
    set_config_var("fps.override", 1);
}

static void cinema_write_set_profile(
    int tier, const char * name,
    int crop, int ro_pct, int res_x, int aspect, int fmt,
    int fps_i, int preview_scale
)
{
    write_profile_tier = tier;
    snprintf(write_profile_name, sizeof(write_profile_name), "%s", name);

    set_config_var("crop.preset", crop);
    set_config_var("crop.lv_res_pct", ro_pct);
    set_config_var("raw.res_x_fine", 0);
    set_config_var("raw.res_x", res_x);
    set_config_var("raw.aspect.ratio", aspect);
    set_config_var("raw.output_format", fmt);
    set_config_var("raw.preview.lv_scale", preview_scale);
    set_config_var("raw.small.hacks", 1);
    set_config_var("raw.video.enabled", 1);
    set_config_var("fps.override", 1);
    set_config_var("fps.override.idx", fps_i);
    fps_request_update();
}

void cinema_write_apply_best_profile(void)
{
    if (!cinema_write_autoprofile) return;

    int speed = write_speed_centi;
    if (speed <= 0)
        speed = get_config_var("raw.write.speed");
    if (speed <= 0) return;

    /* Tier 0: fast CF — 4K 14-bit LJ92 @ 25fps */
    if (speed >= 13000)
    {
        cinema_write_set_profile(0, "4K 14-bit LJ92",
            6, 0, 9, 10, 3, 34, 2);
    }
    /* Tier 1: solid CF — 4K 10-bit @ 25fps */
    else if (speed >= 9500)
    {
        cinema_write_set_profile(1, "4K 10-bit",
            6, 0, 9, 10, 2, 34, 2);
    }
    /* Tier 2: mid — 1080p high FPS */
    else if (speed >= 6500)
    {
        cinema_write_set_profile(2, "1080p 60",
            3, 2, 4, 10, 2, 63, 3);
    }
    /* Tier 3: safe — 1080p 24fps */
    else if (speed >= 4000)
    {
        cinema_write_set_profile(3, "1080p 24",
            0, 0, 4, 10, 3, 33, 1);
    }
    else
    {
        cinema_write_set_profile(4, "720p safe",
            0, 0, 2, 10, 4, 33, 0);
    }

    write_engine_ready = 1;
}

int cinema_write_speed_centi_mbs(void)
{
    int live = get_config_var("raw.write.speed");
    if (live > 0) return live;
    return write_speed_centi;
}

const char * cinema_write_speed_label(void)
{
    static char buf[24];
    int s = cinema_write_speed_centi_mbs();
    if (s <= 0) return "CF: testing...";
    snprintf(buf, sizeof(buf), "CF %d.%02d MB/s", s / 100, s % 100);
    return buf;
}

const char * cinema_write_profile_label(void)
{
    return write_profile_name;
}

int cinema_write_engine_ready(void) { return write_engine_ready; }

void cinema_write_engine_tick(void)
{
    if (!cinema_write_engine_on) return;

    int now = get_ms_clock();

    /* refresh live speed from MLV writes when recording */
    if (RECORDING)
    {
        int live = get_config_var("raw.write.speed");
        if (live > 0) write_speed_centi = live;
        return;
    }

    /* periodic quiet re-probe in LV every 90s */
    if (!lv) return;
    if (now - last_live_probe_ms < 90000) return;
    last_live_probe_ms = now;

    cinema_write_benchmark_quiet();
    if (cinema_write_autoprofile)
        cinema_write_apply_best_profile();
}

static void cinema_write_boot_task(void * unused)
{
    (void) unused;
    msleep(10000);

    cinema_write_arm_hacks();
    msleep(2000);

    cinema_write_benchmark_quiet();
    cinema_write_apply_best_profile();

    struct card_info * card = get_shooting_card();
    if (write_speed_centi > 0 && card)
    {
        NotifyBox(5000,
            "CINEMA WRITE ENGINE\n"
            "%s %d.%02d MB/s\n"
            "Profile: %s\n"
            "Hacks armed — ready to record.",
            card->type,
            write_speed_centi / 100, write_speed_centi % 100,
            write_profile_name
        );
    }
    else
    {
        NotifyBox(4000, "CINEMA WRITE ENGINE\nCard benchmark pending.\nInsert CF and enter LV.");
    }
}

static void cinema_write_service_task(void * unused)
{
    (void) unused;
    while (1)
    {
        msleep(5000);
        cinema_write_engine_tick();
    }
}

static void cinema_write_init(void)
{
    task_create("cine_boot", 0x1c, 0x2000, cinema_write_boot_task, 0);
}

INIT_FUNC(__FILE__, cinema_write_init);
TASK_CREATE("cine_wr_svc", cinema_write_service_task, 0, 0x1e, 0x1000);
