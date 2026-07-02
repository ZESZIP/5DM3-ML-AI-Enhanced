/** \file
 * Cine AI Enhanced — thick in-window setting panels for the CINE page.
 */
#include "dryos.h"
#include "bmp.h"
#include "font.h"
#include "menu.h"
#include "config.h"
#include "propvalues.h"
#include "lens.h"
#include "property.h"
#include "fps.h"
#include "picstyle.h"
#include "beep.h"
#include "cinema_panels.h"
#include "cinema_os.h"
#include "cinema_record_apply.h"
#include "cinema_ui_theme.h"

static int panel_row = -1;
static int panel_sel = 0;

static const char * beast_labels[] = { "BEAST 4K25", "BEAST 1080p120" };
static const char * res_labels[]   = { "720p", "1080p", "2.7K", "4K UHD", "6K" };
static const char * fps_opts[]     = { "24 fps", "25 fps", "50 fps", "60 fps", "100 fps", "120 fps" };
static const char * fmt_opts[] = {
    "MOV (Canon H.264)",
    "MLV 14-bit",
    "MLV 12-bit",
    "MLV 10-bit",
    "MLV LJ92 14-bit",
    "MLV LJ92 12-bit",
    "CINEPACK Stream Pro"
};

static const char * sensor_opts[] = { "Sensor 100%", "Sensor 75%", "Sensor 50%", "Sensor 25%" };
static const int    sensor_vals[]  = { 100, 75, 50, 25 };
static const char * lv_opts[]     = { "LV preview 100%", "LV preview 75%", "LV preview 50%", "LV preview 25%" };
static const int    lv_vals[]     = { 100, 75, 50, 25 };

static const int row_icons[] = {
    ICON_ML_MOVIE, ICON_ML_MOVIE, ICON_ML_MOVIE, ICON_ML_SHOOT,
    ICON_ML_EXPO, ICON_ML_EXPO, ICON_ML_EXPO, ICON_ML_SHOOT, ICON_ML_AUDIO
};

static CONFIG_INT("cine.rec.beast", cine_beast, 0);
static CONFIG_INT("cine.rec.res", cine_res, 1);
static CONFIG_INT("cine.rec.fps", cine_fps, 1);
static CONFIG_INT("cine.rec.fmtidx", cine_fmt_idx, 5);

static int panel_option_count(int row)
{
    switch (row)
    {
        case 0: return COUNT(beast_labels) + COUNT(res_labels) + COUNT(sensor_opts) + COUNT(lv_opts);
        case 1: return COUNT(fps_opts);
        case 2: return COUNT(fmt_opts);
        case 3: return 7;
        case 4: return 5;
        case 5: return 1;
        case 6: return 8;
        case 7: return 5;
        case 8: return 3;
        default: return 0;
    }
}

static void panel_sync_sel(int row)
{
    switch (row)
    {
        case 0:
            panel_sel = cine_beast ? (cine_beast - 1) : (COUNT(beast_labels) + cine_res);
            break;
        case 1: panel_sel = cine_fps; break;
        case 2: panel_sel = cine_fmt_idx; break;
        case 3: panel_sel = COERCE(lens_info.picstyle - 1, 0, 6); break;
        case 4: panel_sel = 2; break;
        case 5: panel_sel = 0; break;
        case 6: panel_sel = 4; break;
        case 7: panel_sel = 1; break;
        case 8: panel_sel = 0; break;
    }
}

static void panel_get_label(int row, int opt, char * buf, int len)
{
    switch (row)
    {
        case 0:
            if (opt < COUNT(beast_labels))
                snprintf(buf, len, "%s", beast_labels[opt]);
            else if (opt < COUNT(beast_labels) + COUNT(res_labels))
                snprintf(buf, len, "%s", res_labels[opt - COUNT(beast_labels)]);
            else if (opt < COUNT(beast_labels) + COUNT(res_labels) + COUNT(sensor_opts))
                snprintf(buf, len, "%s", sensor_opts[opt - COUNT(beast_labels) - COUNT(res_labels)]);
            else
                snprintf(buf, len, "%s", lv_opts[opt - COUNT(beast_labels) - COUNT(res_labels) - COUNT(sensor_opts)]);
            break;
        case 1: snprintf(buf, len, "%s", fps_opts[opt]); break;
        case 2: snprintf(buf, len, "%s", fmt_opts[opt]); break;
        case 3:
        {
            static const char * s[] = {
                "Standard", "Portrait", "Landscape", "Neutral", "Faithful", "Monochrome", "Auto"
            };
            snprintf(buf, len, "%s", s[opt]);
            break;
        }
        case 4:
        {
            static const char * a[] = { "45 deg", "90 deg", "180 deg", "360 deg", "1/50 sec" };
            snprintf(buf, len, "%s", a[opt]);
            break;
        }
        case 5:
            snprintf(buf, len, "Current: %s", lens_format_aperture(lens_info.raw_aperture));
            break;
        case 6:
        {
            static const int iso[] = { 100, 200, 400, 800, 1600, 3200, 6400, 12800 };
            snprintf(buf, len, "ISO %d", iso[opt]);
            break;
        }
        case 7:
        {
            static const char * wb[] = {
                "Auto WB", "5600K Daylight", "3200K Tungsten", "4300K Fluoro", "Custom Kelvin"
            };
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

static int panel_apply(int row, int opt)
{
    switch (row)
    {
        case 0:
        {
            int base = COUNT(beast_labels);
            int res_base = base + COUNT(res_labels);
            int sen_base = res_base + COUNT(sensor_opts);
            if (opt < base)
            {
                cinema_record_set_beast(opt + 1);
                return cinema_record_apply_full();
            }
            if (opt < res_base)
            {
                cinema_record_set_resolution(opt - base);
                return cinema_record_apply_full();
            }
            if (opt < sen_base)
            {
                cinema_record_set_sensor_pct(sensor_vals[opt - res_base]);
                return cinema_record_apply_full();
            }
            cinema_record_set_lv_pct(lv_vals[opt - sen_base]);
            return cinema_record_apply_full();
        }

        case 1:
            cinema_record_set_fps(opt);
            return cinema_record_apply_full();

        case 2:
            cinema_record_set_format_idx(opt);
            return cinema_record_apply_full();

        case 3:
            if (!RECORDING)
            {
                int p = opt + 1;
                p = get_prop_picstyle_from_index(p);
                prop_request_change(PROP_PICTURE_STYLE, &p, 4);
            }
            return 1;

        case 4:
        {
            static const int sh[] = {
                SHUTTER_1_50, SHUTTER_1_100, SHUTTER_1_180, SHUTTER_1_90, SHUTTER_1_50
            };
            lens_set_rawshutter(sh[opt]);
            return 1;
        }

        case 5:
            menu_goto_entry("Movie", "Expo. Override");
            return 1;

        case 6:
        {
            static const int iso[] = { 100, 200, 400, 800, 1600, 3200, 6400, 12800 };
            for (int i = 0; i < ISO_ARRAY_LEN; i++)
            {
                if (raw2iso(codes_iso[i]) == iso[opt])
                {
                    lens_set_rawiso(codes_iso[i]);
                    break;
                }
            }
            return 1;
        }

        case 7:
        {
            static const int k[] = { 0, 5600, 3200, 4300, 5000 };
            if (opt == 4) menu_goto_entry("Shoot", "White Balance");
            else if (opt == 0)
            {
                int m = 0;
                prop_request_change(PROP_WB_MODE_LV, &m, 4);
            }
            else lens_set_kelvin(k[opt]);
            return 1;
        }

        case 8:
            if (opt == 0) menu_goto_entry("Audio", "Analog Gain");
            else if (opt == 1) menu_goto_entry("Audio", "Headphone Mon.");
            else menu_goto_entry("Audio", "Digital Gain");
            return 1;
    }
    return 0;
}

int cinema_panel_is_open(void) { return panel_row >= 0; }

void cinema_panel_open(int row)
{
    if (row < 0 || row > 8) return;
    panel_row = row;
    panel_sync_sel(row);
}

void cinema_panel_close(void) { panel_row = -1; }

void cinema_panel_draw(int y0, int body_h)
{
    if (panel_row < 0) return;

    static const char * titles[] = {
        "RESOLUTION", "FRAME RATE", "RECORD FORMAT", "GAMMA CURVE",
        "SHUTTER", "APERTURE", "ISO / GAIN", "WHITE BALANCE", "AUDIO"
    };

    bmp_fill(COLOR_BLACK, 0, y0 - 8, 720, body_h + 16);
    cine_ui_draw_veil_20(0, y0 - 8, 720, body_h + 16);

    int px = 24;
    int py = y0 + 4;
    int pw = 672;
    int ph = body_h - 8;
    cine_ui_draw_submenu_frame(px, py, pw, ph, CINE_COLOR_CINEMA, titles[panel_row]);

    int list_y = py + 58;
    int row_h = 54;
    int count = panel_option_count(panel_row);
    int visible = MIN(count, 5);
    int scroll = 0;
    if (panel_sel >= visible) scroll = panel_sel - visible + 1;

    for (int v = 0; v < visible; v++)
    {
        int opt = scroll + v;
        if (opt >= count) break;
        int ry = list_y + v * row_h;
        int sel = (opt == panel_sel);
        char label[64];
        panel_get_label(panel_row, opt, label, sizeof(label));

        cine_ui_draw_row_card(px + 16, ry, pw - 32, row_h - 6, CINE_COLOR_CINEMA, sel);
        bfnt_draw_char(row_icons[panel_row], px + 28, ry + 10,
            sel ? CINE_COLOR_CINEMA : COLOR_WHITE, sel ? COLOR_BLACK : CINE_COLOR_CINEMA);
        int fg = sel ? CINE_COLOR_CINEMA : COLOR_WHITE;
        int bg = sel ? COLOR_BLACK : CINE_COLOR_CINEMA;
        bmp_printf(FONT(FONT_LARGE, fg, bg), px + 72, ry + 14, "%s", label);
        if (sel)
            bmp_printf(FONT(FONT_MED, CINE_COLOR_CINEMA, bg), px + pw - 72, ry + 16, "< >");
    }

    bmp_printf(FONT(FONT_SMALL, COLOR_WHITE, COLOR_BLACK), px + 20, py + ph - 28,
        "L/R change   SET apply   MENU back");
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
            panel_apply(panel_row, panel_sel);
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
