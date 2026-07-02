/** \file
 * Cinema OS data-rate governor — adaptive MLV format fallback during recording.
 */
#include "dryos.h"
#include "config.h"
#include "bmp.h"
#include "propvalues.h"
#include "fps.h"

static CONFIG_INT("cinema.governor", cinema_governor_on, 1);

/* MLV output_format indices (mlv_lite) */
#define GOV_FMT_14BIT_NATIVE   0
#define GOV_FMT_14BIT_LJ92     3
#define GOV_FMT_12BIT_LJ92     4

static int governor_fallback = 0;
static int governor_saved_format = -1;

static const int res_presets_x[] = {
    640, 960, 1280, 1600, 1920, 2240, 2560, 2880, 3072, 3520, 4096, 5796
};

static int governor_res_x(void)
{
    int idx = get_config_var("raw.res_x");
    int fine = get_config_var("raw.res_x_fine");
    if (idx < 0 || idx >= COUNT(res_presets_x)) return 1920;
    return res_presets_x[idx] + fine;
}

static int governor_fps_x1000(void)
{
    if (get_config_var("fps.override"))
    {
        int f = fps_get_current_x1000();
        if (f > 0) return f;
    }
    return video_mode_fps * 1000;
}

/* Required write throughput in centi-MB/s (0.01 MB/s units, same as raw.write.speed) */
static int governor_required_centi_mbs(int width, int height, int fmt, int fps_x1000)
{
    int bpp_eff;
    switch (fmt)
    {
        case GOV_FMT_14BIT_LJ92:
        case GOV_FMT_12BIT_LJ92:
            bpp_eff = 9;
            break;
        case GOV_FMT_14BIT_NATIVE:
        default:
            bpp_eff = 14;
            break;
    }

    long long bytes_per_sec = (long long) width * height * bpp_eff * fps_x1000 / 1000 / 8;
    return (int) (bytes_per_sec / 1024 / 1024 * 100);
}

int cinema_governor_fallback_active(void) { return governor_fallback; }

const char * cinema_governor_format_label(void)
{
    static char buf[56];
    int fmt = get_config_var("raw.output_format");

    if (governor_fallback == 1)
        return "12-bit MLV RAW (adaptive)";
    if (governor_fallback == 2)
        return "14-bit LJ92 (adaptive)";

    switch (fmt)
    {
        case GOV_FMT_14BIT_NATIVE: return "14-bit MLV RAW";
        case GOV_FMT_14BIT_LJ92:   return "14-bit MLV RAW (LJ92)";
        case GOV_FMT_12BIT_LJ92:   return "12-bit MLV RAW (LJ92)";
        case 1: return "12-bit MLV RAW";
        case 2: return "10-bit MLV RAW";
        default:
            snprintf(buf, sizeof(buf), "MLV format %d", fmt);
            return buf;
    }
}

void cinema_governor_reset(void)
{
    if (governor_fallback && governor_saved_format >= 0)
        set_config_var("raw.output_format", governor_saved_format);
    governor_fallback = 0;
    governor_saved_format = -1;
}

void cinema_governor_tick(void)
{
    if (!cinema_governor_on) return;
    if (!get_config_var("raw.video.enabled")) return;

    if (!RECORDING)
    {
        if (governor_fallback) cinema_governor_reset();
        return;
    }

    int write_speed = get_config_var("raw.write.speed");
    if (write_speed <= 0) return;

    int width = governor_res_x();
    int height = width * 9 / 16;
    int fmt = get_config_var("raw.output_format");
    int fps = governor_fps_x1000();

    if (width < 3520) return;
    if (fmt != GOV_FMT_14BIT_NATIVE && fmt != GOV_FMT_14BIT_LJ92) return;

    int required = governor_required_centi_mbs(width, height, fmt, fps);
    int margin = required * 92 / 100;

    if (write_speed >= margin) return;

    if (governor_fallback == 0)
    {
        governor_saved_format = fmt;
        if (fmt == GOV_FMT_14BIT_NATIVE)
        {
            set_config_var("raw.output_format", GOV_FMT_14BIT_LJ92);
            governor_fallback = 2;
            NotifyBox(2500, "Governor: LJ92 compression ON");
        }
        else
        {
            set_config_var("raw.output_format", GOV_FMT_12BIT_LJ92);
            governor_fallback = 1;
            NotifyBox(2500, "Governor: 12-bit fallback");
        }
    }
    else if (governor_fallback == 2 && fmt == GOV_FMT_14BIT_LJ92)
    {
        set_config_var("raw.output_format", GOV_FMT_12BIT_LJ92);
        governor_fallback = 1;
        NotifyBox(2500, "Governor: 12-bit fallback");
    }
}
