/** \file
 * Cine AI Enhanced — apply MLV/MOV recording settings for real.
 */
#include "dryos.h"
#include "bmp.h"
#include "config.h"
#include "propvalues.h"
#include "fps.h"
#include "beep.h"
#include "gui.h"
#include "cinema_record_apply.h"
#include "cinema_write_engine.h"

static CONFIG_INT("cine.rec.res", cine_res, 1);
static CONFIG_INT("cine.rec.aspect", cine_aspect, 0);
static CONFIG_INT("cine.rec.depth", cine_depth, 2);
static CONFIG_INT("cine.rec.fps", cine_fps, 1);
static CONFIG_INT("cine.rec.beast", cine_beast, 0);
static CONFIG_INT("cine.rec.fmtidx", cine_fmt_idx, 5); /* MLV LJ92 12-bit default */
static CONFIG_INT("cinema.rec.container", cinema_rec_container, 1);

static const char * res_labels[]   = { "720p", "1080p", "2.7K", "4K", "6K" };
static const int    res_target_w[] = { 1280, 1920, 2704, 3840, 5760 };
static const int    aspect_idx[]   = { 10, 10, 10, 10, 10 };
static const char * fps_labels[]   = { "24", "25", "50", "60", "100", "120" };
static const int    fps_idx[]      = { 33, 34, 60, 63, 75, 77 };

static const char * fmt_labels[] = {
    "MOV (Canon H.264)",
    "MLV 14-bit",
    "MLV 12-bit",
    "MLV 10-bit",
    "MLV LJ92 14-bit",
    "MLV LJ92 12-bit"
};

/* maps fmt_idx -> raw.output_format (MOV ignores) */
static const int fmt_output[] = { -1, 0, 1, 2, 3, 4 };

static const int res_presets[] = {
    640, 960, 1280, 1600, 1920, 2240, 2560, 2880, 3072, 3520, 4096, 5796
};

static int res_x_index(int target_w)
{
    int best = 0;
    int best_diff = 1000000;
    for (int i = 0; i < COUNT(res_presets); i++)
    {
        int d = ABS(res_presets[i] - target_w);
        if (d < best_diff) { best_diff = d; best = i; }
    }
    return best;
}

static int crop_index(int res, int fps_sel)
{
    int hfps = (fps_sel >= 2);
    int xfps = (fps_sel >= 4);
    switch (res)
    {
        case 0:
        case 1:
            return (xfps || hfps) ? 3 : 0;
        case 2: return 5;
        case 3: return hfps ? 7 : 6;
        case 4: return 9;
    }
    return 0;
}

static int readout_pct_index(int fps_sel)
{
    if (fps_sel >= 5) return 3;
    if (fps_sel >= 4) return 2;
    return 0;
}

void cinema_record_set_beast(int beast)
{
    cine_beast = COERCE(beast, 0, 2);
}

void cinema_record_set_resolution(int res_idx)
{
    cine_beast = 0;
    cine_res = COERCE(res_idx, 0, COUNT(res_labels) - 1);
}

void cinema_record_set_fps(int fps_idx)
{
    cine_fps = COERCE(fps_idx, 0, COUNT(fps_labels) - 1);
}

void cinema_record_set_format_idx(int fmt_idx)
{
    cine_fmt_idx = COERCE(fmt_idx, 0, COUNT(fmt_labels) - 1);
    cinema_rec_container = (cine_fmt_idx == 0) ? CINE_REC_MOV : CINE_REC_MLV;
    if (cine_fmt_idx > 0)
        cine_depth = COERCE(cine_fmt_idx - 1, 0, 4);
}

const char * cinema_record_container_label(void)
{
    return cinema_rec_container == CINE_REC_MLV ? "MLV" : "MOV";
}

const char * cinema_record_format_label(void)
{
    int i = COERCE(cine_fmt_idx, 0, COUNT(fmt_labels) - 1);
    return fmt_labels[i];
}

const char * cinema_record_resolution_label(void)
{
    static char buf[32];
    if (cine_beast == 1) return "4K UHD";
    if (cine_beast == 2) return "1080p";
    int r = COERCE(cine_res, 0, COUNT(res_labels) - 1);
    snprintf(buf, sizeof(buf), "%s", res_labels[r]);
    return buf;
}

const char * cinema_record_fps_label(void)
{
    static char buf[24];
    int f = COERCE(cine_fps, 0, COUNT(fps_labels) - 1);
    snprintf(buf, sizeof(buf), "%s fps", fps_labels[f]);
    return buf;
}

int cinema_record_mlv_armed(void)
{
    return cinema_rec_container == CINE_REC_MLV
        && get_config_var("raw.video.enabled");
}

int cinema_record_apply_full(void)
{
    if (RECORDING)
    {
        NotifyBox(2500, "Stop recording first.");
        return 0;
    }

    if (!lv || !is_movie_mode())
    {
        NotifyBox(3000, "Enter movie LiveView first.");
        return 0;
    }

    cine_res    = COERCE(cine_res, 0, COUNT(res_labels) - 1);
    cine_fps    = COERCE(cine_fps, 0, COUNT(fps_labels) - 1);
    cine_fmt_idx = COERCE(cine_fmt_idx, 0, COUNT(fmt_labels) - 1);
    cinema_rec_container = (cine_fmt_idx == 0) ? CINE_REC_MOV : CINE_REC_MLV;

    int crop, ro_pct, res_x, aspect, fmt, fps_i, preview_scale;

    if (cine_beast == 1)
    {
        crop = 6; ro_pct = 0; res_x = 9; aspect = 10; fmt = 4; fps_i = 34;
        preview_scale = 2;
        cine_fmt_idx = 5;
        cinema_rec_container = CINE_REC_MLV;
    }
    else if (cine_beast == 2)
    {
        crop = 3; ro_pct = 2; res_x = 4; aspect = 10; fmt = 4; fps_i = 77;
        preview_scale = 3;
        cine_fmt_idx = 5;
        cinema_rec_container = CINE_REC_MLV;
        NotifyBox(3500, "High FPS: set Canon menu to 720p 50/60.");
    }
    else
    {
        crop = crop_index(cine_res, cine_fps);
        ro_pct = readout_pct_index(cine_fps);
        res_x = res_x_index(res_target_w[cine_res]);
        aspect = aspect_idx[cine_res];
        fps_i = fps_idx[cine_fps];
        preview_scale = (cine_fps >= 4) ? 3 : (cine_fps >= 2) ? 2 : 0;

        if (cinema_rec_container == CINE_REC_MLV)
        {
            int fi = COERCE(cine_fmt_idx, 1, COUNT(fmt_output) - 1);
            fmt = fmt_output[fi];
            if (fmt < 0) fmt = 4;
        }
        else
            fmt = 4;
    }

    /* Force MLV subsystem to re-init by toggling off first */
    set_config_var("raw.video.enabled", 0);
    msleep(250);

    set_config_var("crop.preset", crop);
    msleep(500);
    set_config_var("crop.lv_res_pct", ro_pct);
    set_config_var("raw.res_x_fine", 0);
    set_config_var("raw.res_x", res_x);
    set_config_var("raw.aspect.ratio", aspect);
    set_config_var("fps.override", 1);
    set_config_var("fps.override.idx", fps_i);
    fps_request_update();

    if (cinema_rec_container == CINE_REC_MLV)
    {
        set_config_var("raw.h264.proxy", 0);
        set_config_var("raw.output_format", fmt);
        set_config_var("raw.preview.lv_scale", preview_scale);
        set_config_var("raw.preview", 2);
        set_config_var("raw.small.hacks", 1);
        set_config_var("raw.use.srm.memory", 1);
        set_config_var("raw.sync_beep", 1);
        set_config_var("raw.video.enabled", 1);
        msleep(600);
        cinema_write_arm_hacks();
        /* do not call apply_best_profile here — it would override user format/res */

        NotifyBox(4000,
            "MLV armed: %s\n%s @ %s\nPress REC to record MLV.",
            fmt_labels[cine_fmt_idx],
            cinema_record_resolution_label(),
            cinema_record_fps_label());
        beep();
    }
    else
    {
        set_config_var("raw.video.enabled", 0);
        set_config_var("raw.h264.proxy", 0);
        NotifyBox(3000, "MOV mode.\nPress REC for Canon H.264.");
    }

    config_save();
    return 1;
}
