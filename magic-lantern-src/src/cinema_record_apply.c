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
#include "property.h"
#include "module.h"
#include "cinema_record_apply.h"
#include "cinema_write_engine.h"
#include "cinema_module_bridge.h"
#include "cine_codec.h"
#include "cinema_debug.h"

static CONFIG_INT("cine.rec.res", cine_res, 1);
static CONFIG_INT("cine.rec.fps", cine_fps, 1);
static CONFIG_INT("cine.rec.beast", cine_beast, 0);
static CONFIG_INT("cine.rec.fmtidx", cine_fmt_idx, 1);
static CONFIG_INT("cinema.rec.container", cinema_rec_container, 1);
static CONFIG_INT("cine.sensor.pct", cine_sensor_pct, 100);
static CONFIG_INT("cine.lv.pct", cine_lv_pct, 50);
static CONFIG_INT("cine.rec.bpp", cine_bpp_idx, 0);
static CONFIG_INT("cine.rec.peaking", cine_peaking_on, 1);

static const char * res_labels[]   = { "720p", "1080p", "2.7K", "4K UHD", "6K" };
static const int    res_target_w[] = { 1280, 1920, 2704, 3840, 5760 };
static const char * fps_labels[]   = { "24", "25", "50", "60", "100", "120" };
static const int    fps_idx[]      = { 33, 34, 60, 63, 75, 77 };

static const char * fmt_labels[] = {
    "MOV (Canon H.264)",
    "CINEPACK Stream Pro"
};

#define CINE_FMT_MOV       0
#define CINE_FMT_CINEPACK  1
#define CINE_RAW_FMT_ARM   4  /* legacy; bpp idx selects raw.output_format */

/* raw.output_format indices for CINEPACK source depth */
static const int bpp_output_fmt[] = { 3, 4, 2, 5 }; /* 14, 12, 10, 8-bit */
static const char * bpp_labels[]  = { "14-bit", "12-bit", "10-bit", "8-bit" };

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
        case 1: return (xfps || hfps) ? 3 : 0;
        case 2: return 5;
        case 3: return hfps ? 7 : 6;
        case 4: return 9;
    }
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
  if (fmt_idx > CINE_FMT_CINEPACK)
        fmt_idx = CINE_FMT_CINEPACK; /* migrate legacy MLV indices */
    cine_fmt_idx = COERCE(fmt_idx, 0, COUNT(fmt_labels) - 1);
    cinema_rec_container = (cine_fmt_idx == CINE_FMT_MOV) ? CINE_REC_MOV : CINE_REC_MLV;
}

int cinema_record_format_is_cinepack(void)
{
    return COERCE(cine_fmt_idx, 0, COUNT(fmt_labels) - 1) == CINE_FMT_CINEPACK;
}

void cinema_record_set_sensor_pct(int pct)
{
    cine_sensor_pct = COERCE(pct, 25, 100);
    cine_codec_set_sensor_pct(cine_sensor_pct);
}

void cinema_record_set_lv_pct(int pct)
{
    cine_lv_pct = COERCE(pct, 25, 100);
    cine_codec_set_lv_pct(cine_lv_pct);
}

int cinema_record_lv_pct(void)
{
    return COERCE(cine_lv_pct, 25, 100);
}

void cinema_record_set_bpp_idx(int idx)
{
    cine_bpp_idx = COERCE(idx, 0, COUNT(bpp_labels) - 1);
    set_config_var("cine.rec.bpp", cine_bpp_idx);
}

int cinema_record_bpp_idx(void)
{
    return COERCE(cine_bpp_idx, 0, COUNT(bpp_labels) - 1);
}

const char * cinema_record_bpp_label(void)
{
    return bpp_labels[cinema_record_bpp_idx()];
}

void cinema_record_set_peaking(int on)
{
    cine_peaking_on = on ? 1 : 0;
    set_config_var("focus.peaking", cine_peaking_on);
    if (cine_peaking_on)
    {
        set_config_var("focus.peaking.disp", 2);
        set_config_var("focus.peaking.thr", 4);
    }
}

int cinema_record_peaking_on(void)
{
    return cine_peaking_on;
}

static void cinema_record_apply_peaking(void)
{
    set_config_var("focus.peaking", cine_peaking_on);
    if (cine_peaking_on)
    {
        set_config_var("focus.peaking.disp", 2);
        set_config_var("focus.peaking.thr", 4);
    }
}

const char * cinema_record_container_label(void)
{
    if (cinema_rec_container == CINE_REC_MOV)
        return "MOV";
    return cinema_record_format_is_cinepack() ? "CSP" : "RAW";
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
    cine_debug_log("apply_full start");

    if (RECORDING)
    {
        cine_debug_log("fail: recording active");
        NotifyBox(2500, "Stop recording first.");
        return 0;
    }

    if (!lv || !is_movie_mode())
    {
        cine_debug_log("fail: lv=%d movie=%d", lv ? 1 : 0, is_movie_mode() ? 1 : 0);
        NotifyBox(3000, "Enter movie LiveView first.");
        return 0;
    }

    cine_res     = COERCE(cine_res, 0, COUNT(res_labels) - 1);
    cine_fps     = COERCE(cine_fps, 0, COUNT(fps_labels) - 1);
    cine_fmt_idx = COERCE(cine_fmt_idx, 0, COUNT(fmt_labels) - 1);
    if (cine_fmt_idx > CINE_FMT_CINEPACK)
        cine_fmt_idx = CINE_FMT_CINEPACK;
    cinema_rec_container = (cine_fmt_idx == CINE_FMT_MOV) ? CINE_REC_MOV : CINE_REC_MLV;

    int use_cinepack = cinema_record_format_is_cinepack();
    int crop, ro_pct, res_x, aspect, fmt, fps_i, preview_scale;
    int bpp_i = cinema_record_bpp_idx();

    cine_codec_set_mode(use_cinepack, 90);
    set_config_var("cine.codec.pack", use_cinepack ? 1 : 0);
    set_config_var("cine.codec.quality", 90);
    cine_codec_set_sensor_pct(cine_sensor_pct);
    cine_codec_set_lv_pct(cine_lv_pct);
    cine_codec_set_record_profile(cine_res, cine_fps, cine_beast);
    ro_pct = cine_codec_readout_pct_index();
    preview_scale = cine_codec_lv_scale_index();

    if (cine_beast == 1)
    {
        crop = 6; res_x = 9; aspect = 10; fmt = bpp_output_fmt[bpp_i]; fps_i = 34;
        cine_fmt_idx = CINE_FMT_CINEPACK;
        cinema_rec_container = CINE_REC_MLV;
    }
    else if (cine_beast == 2)
    {
        crop = 3; ro_pct = 2; res_x = 4; aspect = 10; fmt = bpp_output_fmt[bpp_i]; fps_i = 77;
        preview_scale = 3;
        cine_fmt_idx = CINE_FMT_CINEPACK;
        cinema_rec_container = CINE_REC_MLV;
        NotifyBox(3500, "High FPS: Canon menu 720p 50/60.");
    }
    else
    {
        crop = crop_index(cine_res, cine_fps);
        res_x = res_x_index(res_target_w[cine_res]);
        aspect = 10;
        fps_i = fps_idx[cine_fps];
        if (cinema_rec_container == CINE_REC_MLV)
            fmt = bpp_output_fmt[bpp_i];
        else
            fmt = bpp_output_fmt[bpp_i];
    }

    cinema_record_apply_peaking();

    gui_uilock(UILOCK_EVERYTHING);

    cine_debug_log("crop=%d ro=%d res_x=%d fmt=%d prev=%d",
        crop, ro_pct, res_x, fmt, preview_scale);

    if (!cinema_bridge_crop_apply(crop, ro_pct))
    {
        cine_debug_log("crop bridge fail, fallback set_config");
        set_config_var("crop.preset", crop);
        set_config_var("crop.lv_res_pct", ro_pct);
        msleep(800);
        if (!cinema_bridge_crop_apply(crop, ro_pct))
        {
            cine_debug_log("crop bridge fail FINAL");
            cine_debug_flush();
            NotifyBox(3500, "Enable crop_rec module\nfor resolution changes.");
        }
    }
    else
        cine_debug_log("crop bridge OK");

    set_config_var("fps.override", 1);
    set_config_var("fps.override.idx", fps_i);
    fps_request_update();
    msleep(300);

    if (cinema_rec_container == CINE_REC_MLV)
    {
        if (!cinema_bridge_mlv_arm(1, res_x, aspect, fmt, preview_scale))
        {
            cine_debug_log("mlv bridge fail, fallback set_config");
            set_config_var("raw.video.enabled", 0);
            msleep(200);
            set_config_var("raw.res_x", res_x);
            set_config_var("raw.res_x_fine", 0);
            set_config_var("raw.aspect.ratio", aspect);
            set_config_var("raw.output_format", fmt);
            set_config_var("raw.preview.lv_scale", preview_scale);
            set_config_var("raw.h264.proxy", 0);
            set_config_var("raw.video.enabled", 1);
            msleep(800);
            if (!cinema_bridge_mlv_arm(1, res_x, aspect, fmt, preview_scale))
            {
                cine_debug_log("mlv bridge fail FINAL");
                cine_debug_log_modules();
                cine_debug_flush();
                NotifyBox(4000, "Enable mlv_lite module\nSee ML/LOGS/CINE_DEBUG.LOG");
            }
        }
        else
            cine_debug_log("mlv bridge OK");

        cinema_write_arm_hacks();

        if (!get_config_var("raw.video.enabled"))
        {
            gui_uilock(UILOCK_NONE);
            cine_debug_log("MLV arm FAILED raw.video.enabled=0");
            cine_debug_log_mlv_state();
            cine_debug_flush();
            NotifyBox(5000, "MLV arm FAILED.\nUpload ML/LOGS/CINE_DEBUG.LOG");
            return 0;
        }

        NotifyBox(4500,
            "%s armed\n%s @ %s\nSensor %d%%  LV %d%%\n%s",
            fmt_labels[cine_fmt_idx],
            cinema_record_resolution_label(),
            cinema_record_fps_label(),
            cine_sensor_pct, cine_lv_pct,
            use_cinepack ? "REC writes .CIX — PC: cinepack_to_mlv.py" : "Press REC for MLV.");
        beep();
    }
    else
    {
        cinema_bridge_mlv_arm(0, 0, 0, 0, 0);
        set_config_var("raw.video.enabled", 0);
        cine_codec_set_mode(0, 85);
        NotifyBox(3000, "MOV mode.\nPress REC for Canon H.264.");
    }

    gui_uilock(UILOCK_NONE);
    config_save();
    cine_debug_log("apply_full done ok=%d", cinema_rec_container == CINE_REC_MLV);
    cine_debug_log_mlv_state();
    cine_debug_flush();
    return 1;
}
