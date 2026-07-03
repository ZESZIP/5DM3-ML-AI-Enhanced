/** \file
 * Cinema OS data-rate governor — adaptive throughput for 4K MLV / CSP.
 * Never stops recording; scales bitrate, bit depth, or LJ92 on the fly.
 */
#include "dryos.h"
#include "config.h"
#include "bmp.h"
#include "propvalues.h"
#include "fps.h"
#include "module.h"

static CONFIG_INT("cinema.governor", cinema_governor_on, 1);

#define GOV_FMT_14BIT_NATIVE   0
#define GOV_FMT_14BIT_LJ92     3
#define GOV_FMT_12BIT_LJ92     4

static int governor_fallback = 0;
static int governor_saved_format = -1;
static int governor_csp_tier = 0;
static char governor_label[64] = "14-bit MLV RAW";

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

static int governor_required_centi_mbs(int width, int height, int fmt, int fps_x1000)
{
    int bpp_eff = 14;
    switch (fmt)
    {
        case GOV_FMT_14BIT_LJ92:
        case GOV_FMT_12BIT_LJ92: bpp_eff = 9; break;
        case 2: bpp_eff = 10; break;
        case 5: bpp_eff = 8; break;
        default: bpp_eff = 14; break;
    }
    long long bps = (long long) width * height * bpp_eff * fps_x1000 / 1000 / 8;
    return (int) (bps / 1024 / 1024 * 100);
}

static void governor_set_label(const char * s)
{
    snprintf(governor_label, sizeof(governor_label), "%s", s);
}

static void governor_csp_adjust(int tier)
{
    static const int targets[] = { 10000, 8500, 7200, 5800 };
    if (tier >= COUNT(targets)) tier = COUNT(targets) - 1;
    governor_csp_tier = tier;
    int t = targets[tier];
    set_config_var("cine.codec.target_centi_mbs", t);
    module_exec(0, "mlv_lite_cine_adjust_target", 1, (uint32_t) t);
    static const char * labels[] = {
        "CSP Stream Pro",
        "CSP adaptive (-15%)",
        "CSP adaptive (12-bit)",
        "CSP adaptive (min)"
    };
    governor_set_label(labels[tier]);
}

int cinema_governor_fallback_active(void)
{
    return governor_fallback || governor_csp_tier > 0;
}

const char * cinema_governor_format_label(void)
{
    return governor_label;
}

void cinema_governor_reset(void)
{
    if (governor_fallback && governor_saved_format >= 0)
        set_config_var("raw.output_format", governor_saved_format);
    governor_fallback = 0;
    governor_saved_format = -1;
    governor_csp_tier = 0;
    governor_set_label("14-bit MLV RAW");
}

void cinema_governor_tick(void)
{
    if (!cinema_governor_on) return;
    if (!get_config_var("raw.video.enabled")) return;

    if (!RECORDING)
    {
        if (governor_fallback || governor_csp_tier)
            cinema_governor_reset();
        return;
    }

    int write_speed = get_config_var("raw.write.speed");
    if (write_speed <= 0) return;

    int width = governor_res_x();
    int height = width * 9 / 16;
    int fmt = get_config_var("raw.output_format");
    int fps = governor_fps_x1000();
    int pack = get_config_var("cine.codec.pack");

    if (pack)
    {
        int required = governor_required_centi_mbs(width, height, fmt, fps);
        int margin = required * 88 / 100;
        if (write_speed >= margin && governor_csp_tier == 0)
        {
            governor_set_label("CSP Stream Pro");
            return;
        }
        if (governor_csp_tier < 3 && write_speed < margin)
        {
            governor_csp_adjust(governor_csp_tier + 1);
            NotifyBox(2000, "CSP: %s", governor_label);
        }
        return;
    }

    if (width < 3520) return;

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
            governor_set_label("14-bit LJ92 (adaptive)");
            NotifyBox(2500, "Governor: LJ92 ON");
        }
        else
        {
            set_config_var("raw.output_format", GOV_FMT_12BIT_LJ92);
            governor_fallback = 1;
            governor_set_label("12-bit MLV RAW (adaptive)");
            NotifyBox(2500, "Governor: 12-bit");
        }
    }
    else if (governor_fallback == 2)
    {
        set_config_var("raw.output_format", GOV_FMT_12BIT_LJ92);
        governor_fallback = 1;
        governor_set_label("12-bit MLV RAW (adaptive)");
        NotifyBox(2500, "Governor: 12-bit");
    }
}

/* Called from mlv_lite when write queue backs up */
void cinema_governor_buffer_cleanse(int dropped)
{
    if (!cinema_governor_on || !RECORDING) return;
    if (dropped && (dropped % 8) == 0)
        NotifyBox(1000, "Buffer cleanse: %d", dropped);
}
