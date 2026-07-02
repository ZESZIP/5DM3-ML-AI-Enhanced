/** \file
 * Cine AI Enhanced — thick in-window setting panels for the CINE page.
 */
#include "dryos.h"
#include "bmp.h"
#include "font.h"
#include "menu.h"
#include "config.h"
#include "propvalues.h"
#include "property.h"
#include "lens.h"
#include "fps.h"
#include "picstyle.h"
#include "beep.h"
#include "cinema_panels.h"
#include "cinema_governor.h"
#include "cinema_write_engine.h"

static int panel_row = -1;
static int panel_sel = 0;

/* ---- recording presets (shared with cine_record logic) ---- */

static const char * beast_labels[] = { "MANUAL", "BEAST 4K25", "BEAST 1080p120" };
static const char * res_labels[]   = { "720p", "1080p", "2.7K", "4K", "6K" };
static const int    res_target_w[] = { 1280, 1920, 2704, 3840, 5760 };
static const char * aspect_labels[] = { "16:9", "2.39:1", "2.35:1", "2:1", "4:3", "1:1" };
static const int    aspect_idx[]    = { 10, 5, 6, 8, 13, 16 };
static const char * depth_labels[] = { "14-bit", "12-bit", "10-bit", "8-bit" };
static const int    depth_fmt[]    = { 3, 4, 2, 5 };
static const char * fps_labels[] = { "24", "25", "50", "60", "100", "120" };
static const int    fps_idx[]    = { 33, 34, 60, 63, 75, 77 };

static CONFIG_INT("cine.rec.res", cine_res, 1);
static CONFIG_INT("cine.rec.aspect", cine_aspect, 0);
static CONFIG_INT("cine.rec.depth", cine_depth, 0);
static CONFIG_INT("cine.rec.fps", cine_fps, 0);
static CONFIG_INT("cine.rec.beast", cine_beast, 0);

static const int res_presets[] = {
    640, 960, 1280, 1600, 1920, 2240, 2560, 2880, 3072, 3520, 4096, 5796
};

static int panel_option_count(int row)
{
    switch (row)
    {
        case 0: return COUNT(beast_labels) + COUNT(res_labels);
        case 1: return COUNT(fps_labels);
        case 2: return COUNT(depth_labels);
        case 3: return 7; /* picture styles */
        case 4: return 5; /* shutter angles */
        case 5: return 1; /* aperture info row */
        case 6: return 8; /* ISO stops */
        case 7: return 5; /* WB presets */
        case 8: return 3; /* audio */
        default: return 0;
    }
}

static int cine_res_x_index(int target_w)
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

static int cine_crop_index(int res, int fps_sel)
{
    int hfps = (fps_sel >= 2);
    int xfps = (fps_sel >= 4);
    switch (res)
    {
        case 0:
        case 1:
            if (xfps) return 3;
            if (hfps) return 3;
            return 0;
        case 2: return 5;
        case 3: return hfps ? 7 : 6;
        case 4: return 9;
    }
    return 0;
}

static int cine_readout_pct_index(int fps_sel)
{
    if (fps_sel >= 5) return 3;
    if (fps_sel >= 4) return 2;
    return 0;
}

static int cine_apply_recording_profile(void)
{
    if (RECORDING)
    {
        NotifyBox(2000, "Stop recording first.");
        return 0;
    }
    if (!lv || !is_movie_mode())
    {
        NotifyBox(2000, "Enter movie LiveView first.");
        return 0;
    }

    cine_beast  = COERCE(cine_beast, 0, COUNT(beast_labels) - 1);
    cine_res    = COERCE(cine_res, 0, COUNT(res_labels) - 1);
    cine_aspect = COERCE(cine_aspect, 0, COUNT(aspect_labels) - 1);
    cine_depth  = COERCE(cine_depth, 0, COUNT(depth_labels) - 1);
    cine_fps    = COERCE(cine_fps, 0, COUNT(fps_labels) - 1);

    int crop, ro_pct, res_x, aspect, fmt, fps_i, preview_scale;

    if (cine_beast == 1)
    {
        crop = 6; ro_pct = 0; res_x = 9; aspect = 10; fmt = 2; fps_i = 34;
        preview_scale = 2;
        cine_res = 3; cine_aspect = 0; cine_depth = 2; cine_fps = 1;
    }
    else if (cine_beast == 2)
    {
        crop = 3; ro_pct = 2; res_x = 4; aspect = 10; fmt = 2; fps_i = 77;
        preview_scale = 3;
        cine_res = 1; cine_aspect = 0; cine_depth = 2; cine_fps = 5;
        NotifyBox(3000, "Set Canon menu to 720p 50/60 for high FPS.");
    }
    else
    {
        crop = cine_crop_index(cine_res, cine_fps);
        ro_pct = cine_readout_pct_index(cine_fps);
        res_x = cine_res_x_index(res_target_w[cine_res]);
        aspect = aspect_idx[cine_aspect];
        fmt = depth_fmt[cine_depth];
        fps_i = fps_idx[cine_fps];
        preview_scale = (cine_fps >= 4) ? 3 : (cine_fps >= 2) ? 2 : 0;
    }

    set_config_var("crop.preset", crop);
    msleep(400);
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
    cinema_write_apply_best_profile();
    beep();
    return 1;
}

static void panel_sync_sel_from_config(int row)
{
    switch (row)
    {
        case 0:
            if (cine_beast) panel_sel = cine_beast;
            else panel_sel = COUNT(beast_labels) + cine_res;
            break;
        case 1: panel_sel = cine_fps; break;
        case 2: panel_sel = cine_depth; break;
        case 3:
        {
            int ps = lens_info.picstyle;
            panel_sel = COERCE(ps - 1, 0, 6);
            break;
        }
        case 4: panel_sel = 2; break; /* default 180 deg */
        case 5: panel_sel = 0; break;
        case 6: panel_sel = 4; break; /* ~800 ISO default slot */
        case 7: panel_sel = 1; break;
        case 8: panel_sel = 0; break;
    }
}

static void panel_get_option_label(int row, int opt, char * buf, int len)
{
    switch (row)
    {
        case 0:
            if (opt < COUNT(beast_labels))
                snprintf(buf, len, "%s", beast_labels[opt]);
            else
                snprintf(buf, len, "%s", res_labels[opt - COUNT(beast_labels)]);
            break;
        case 1:
            snprintf(buf, len, "%s fps", fps_labels[opt]);
            break;
        case 2:
            snprintf(buf, len, "%s MLV", depth_labels[opt]);
            break;
        case 3:
        {
            static const char * styles[] = {
                "Standard", "Portrait", "Landscape", "Neutral", "Faithful", "Monochrome", "Auto"
            };
            snprintf(buf, len, "%s", styles[opt]);
            break;
        }
        case 4:
        {
            static const char * angles[] = { "45 deg", "90 deg", "180 deg", "360 deg", "1/48 sec" };
            snprintf(buf, len, "%s", angles[opt]);
            break;
        }
        case 5:
            snprintf(buf, len, "Current: %s", lens_format_aperture(lens_info.raw_aperture));
            break;
        case 6:
        {
            static const int isos[] = { 100, 200, 400, 800, 1600, 3200, 6400, 12800 };
            snprintf(buf, len, "ISO %d", isos[opt]);
            break;
        }
        case 7:
        {
            static const char * wb[] = { "Auto WB", "5600K Daylight", "3200K Tungsten", "4300K Fluoro", "Custom Kelvin" };
            snprintf(buf, len, "%s", wb[opt]);
            break;
        }
        case 8:
        {
            static const char * aud[] = { "Analog Gain", "Headphone Mon.", "Digital Gain" };
            snprintf(buf, len, "%s", aud[opt]);
            break;
        }
        default:
            snprintf(buf, len, "-");
            break;
    }
}

static int panel_apply_option(int row, int opt)
{
    switch (row)
    {
        case 0:
            if (opt < COUNT(beast_labels))
            {
                cine_beast = opt;
                if (opt) return cine_apply_recording_profile();
            }
            else
            {
                cine_beast = 0;
                cine_res = opt - COUNT(beast_labels);
                return cine_apply_recording_profile();
            }
            break;

        case 1:
            cine_beast = 0;
            cine_fps = opt;
            set_config_var("fps.override", 1);
            set_config_var("fps.override.idx", fps_idx[opt]);
            fps_request_update();
            return cine_apply_recording_profile();

        case 2:
            cine_beast = 0;
            cine_depth = opt;
            set_config_var("raw.output_format", depth_fmt[opt]);
            return 1;

        case 3:
        {
            if (RECORDING) return 0;
            int p = opt + 1;
            p = get_prop_picstyle_from_index(p);
            prop_request_change(PROP_PICTURE_STYLE, &p, 4);
            return 1;
        }

        case 4:
        {
            static const int shutters[] = {
                SHUTTER_1_50, SHUTTER_1_100, SHUTTER_1_180, SHUTTER_1_90, SHUTTER_1_200
            };
            if (opt < COUNT(shutters))
                lens_set_rawshutter(shutters[opt]);
            return 1;
        }

        case 5:
            menu_goto_entry("Movie", "Expo. Override");
            return 1;

        case 6:
        {
            static const int isos[] = { 100, 200, 400, 800, 1600, 3200, 6400, 12800 };
            int target = isos[opt];
            for (int i = 0; i < ISO_ARRAY_LEN; i++)
            {
                if (raw2iso(codes_iso[i]) == target)
                {
                    lens_set_rawiso(codes_iso[i]);
                    break;
                }
            }
            return 1;
        }

        case 7:
        {
            static const int kelvins[] = { 0, 5600, 3200, 4300, 5000 };
            if (opt == 0)
            {
                int m = 0;
                prop_request_change(PROP_WB_MODE_LV, &m, 4);
            }
            else if (opt == 4)
                menu_goto_entry("Shoot", "White Balance");
            else
                lens_set_kelvin(kelvins[opt]);
            return 1;
        }

        case 8:
            if (opt == 0)
                menu_goto_entry("Audio", "Analog Gain");
            else if (opt == 1)
                menu_goto_entry("Audio", "Headphone Mon.");
            else
                menu_goto_entry("Audio", "Digital Gain");
            return 1;
    }
    return 0;
}

int cinema_panel_is_open(void) { return panel_row >= 0; }

void cinema_panel_open(int row)
{
    if (row < 0 || row > 8) return;
    panel_row = row;
    panel_sync_sel_from_config(row);
}

void cinema_panel_close(void) { panel_row = -1; }

static void panel_draw_header(const char * title, int y0)
{
    bmp_fill(COLOR_BLACK, 40, y0, 640, 52);
    bmp_fill(COLOR_ORANGE, 40, y0, 640, 6);
    bmp_printf(FONT(FONT_CANON, COLOR_WHITE, COLOR_BLACK), 56, y0 + 14, "%s", title);
    bmp_draw_rect(COLOR_WHITE, 40, y0, 640, 52);
}

static void panel_draw_row(int x, int y, int w, int h, int sel, const char * label)
{
    if (sel)
    {
        bmp_fill(COLOR_WHITE, x, y, w, h);
        bmp_fill(COLOR_GRAY(45), x + 4, y + 4, w - 8, h - 8);
    }
    else
        bmp_fill(COLOR_GRAY(25), x + 4, y + 2, w - 8, h - 4);

    int fg = sel ? COLOR_ORANGE : COLOR_WHITE;
    int bg = sel ? COLOR_GRAY(45) : COLOR_GRAY(25);
    bmp_printf(FONT(FONT_LARGE, fg, bg), x + 20, y + 14, "%s", label);
    if (sel)
        bmp_printf(FONT(FONT_MED, COLOR_ORANGE, bg), x + w - 48, y + 16, "<>");
}

void cinema_panel_draw(int y0, int body_h)
{
    if (panel_row < 0) return;

    static const char * titles[] = {
        "RESOLUTION", "FRAME RATE", "CODEC / FORMAT", "GAMMA CURVE",
        "SHUTTER", "APERTURE", "ISO / GAIN", "WHITE BALANCE", "AUDIO"
    };

    int overlay_y = y0 - 8;
    int overlay_h = body_h + 16;
    bmp_fill(COLOR_GRAY(10), 0, overlay_y, 720, overlay_h);

    int panel_y = overlay_y + 12;
    int panel_h = overlay_h - 24;
    bmp_fill(COLOR_BLACK, 32, panel_y, 656, panel_h);
    bmp_draw_rect(COLOR_WHITE, 32, panel_y, 656, panel_h);
    bmp_draw_rect(COLOR_ORANGE, 34, panel_y + 2, 652, panel_h - 4);

    panel_draw_header(titles[panel_row], panel_y + 8);

    int list_y = panel_y + 68;
    int row_h = 52;
    int count = panel_option_count(panel_row);
    int visible = MIN(count, 5);
    int scroll = 0;
    if (panel_sel >= visible)
        scroll = panel_sel - visible + 1;

    for (int v = 0; v < visible; v++)
    {
        int opt = scroll + v;
        if (opt >= count) break;
        char label[64];
        panel_get_option_label(panel_row, opt, label, sizeof(label));
        panel_draw_row(48, list_y + v * row_h, 624, row_h - 4,
            opt == panel_sel, label);
    }

    int foot = panel_y + panel_h - 44;
    bmp_fill(COLOR_BLACK, 48, foot, 624, 36);
    bmp_printf(FONT(FONT_SMALL, COLOR_GRAY(55), COLOR_BLACK), 56, foot + 10,
        "Wheel L/R change   SET apply   MENU back");
}

int cinema_panel_handle_key(unsigned int key)
{
    if (panel_row < 0) return 0;

    int count = panel_option_count(panel_row);

    switch (key)
    {
        case BGMT_WHEEL_LEFT:
        case BGMT_PRESS_LEFT:
        case BGMT_WHEEL_UP:
        case BGMT_PRESS_UP:
            panel_sel = MOD(panel_sel - 1, count);
            return 1;

        case BGMT_WHEEL_RIGHT:
        case BGMT_PRESS_RIGHT:
        case BGMT_WHEEL_DOWN:
        case BGMT_PRESS_DOWN:
            panel_sel = MOD(panel_sel + 1, count);
            return 1;

        case BGMT_PRESS_SET:
            panel_apply_option(panel_row, panel_sel);
            cinema_panel_close();
            menu_redraw();
            return 1;

        case BGMT_MENU:
        case BGMT_TRASH:
        case BGMT_PLAY:
            cinema_panel_close();
            menu_redraw();
            return 1;

        default:
            return 0;
    }
}
