/* Sony-style Cinema Record UI — unified scroll control for MLV recording (5D3) */

#include <module.h>
#include <dryos.h>
#include <bmp.h>
#include <menu.h>
#include <config.h>
#include <property.h>
#include <propvalues.h>
#include <fps.h>
#include <lvinfo.h>
#include <gui.h>
#include <beep.h>

/* resolution tiers (target horizontal pixels) */
static CONFIG_INT("cine.rec.res", cine_res, 1);
static CONFIG_INT("cine.rec.aspect", cine_aspect, 0);
static CONFIG_INT("cine.rec.depth", cine_depth, 0);
static CONFIG_INT("cine.rec.fps", cine_fps, 0);
static CONFIG_INT("cine.rec.anam", cine_anam, 0);
static CONFIG_INT("cine.rec.field", cine_field, 0);
static CONFIG_INT("cine.rec.beast", cine_beast, 0);
static CONFIG_INT("cine.rec.auto", cine_auto_apply, 1);

static const char * cine_beast_labels[] = { "MANUAL", "BEAST 4K25", "BEAST 1080p120" };

static const char * cine_res_labels[]   = { "720p", "1080p", "2.7K", "4K", "6K" };
static const int    cine_res_target_w[] = { 1280, 1920, 2704, 3840, 5760 };

static const char * cine_aspect_labels[] = { "16:9", "2.39:1", "2.35:1", "2:1", "4:3", "1:1" };
static const int    cine_aspect_idx[]    = { 10, 5, 6, 8, 13, 16 };

static const char * cine_depth_labels[] = { "14-bit", "12-bit", "10-bit", "8-bit" };
static const int    cine_depth_fmt[]    = { 3, 4, 2, 5 };

static const char * cine_fps_labels[] = { "24", "25", "50", "60", "100", "120" };
static const int    cine_fps_idx[]    = { 33, 34, 60, 63, 75, 77 };

static const char * cine_anam_labels[] = { "OFF", "2x squeeze" };

#define CINE_FIELD_BEAST  (-1)
#define CINE_FIELD_RES    0
#define CINE_FIELD_ASPECT 1
#define CINE_FIELD_DEPTH  2
#define CINE_FIELD_FPS    3
#define CINE_FIELD_ANAM   4

static const int cine_field_order[] = {
    CINE_FIELD_BEAST,
    CINE_FIELD_RES, CINE_FIELD_ASPECT, CINE_FIELD_DEPTH,
    CINE_FIELD_FPS, CINE_FIELD_ANAM
};
#define CINE_FIELD_ROWS COUNT(cine_field_order)

static int cine_record_active = 0;
static int cine_apply_pending = 0;
static int cine_last_apply_ms = 0;

static int cine_res_x_index(int target_w)
{
    static const int presets[] = {
        640, 960, 1280, 1600, 1920, 2240, 2560, 2880, 3072, 3520, 4096, 5796
    };
    int best = 0;
    int best_diff = 1000000;
    for (int i = 0; i < COUNT(presets); i++)
    {
        int d = ABS(presets[i] - target_w);
        if (d < best_diff)
        {
            best_diff = d;
            best = i;
        }
    }
    return best;
}

static int cine_crop_index(int res, int fps_sel)
{
    int hfps = (fps_sel >= 2);
    int xfps = (fps_sel >= 4);

    switch (res)
    {
        case 0: /* 720p */
        case 1: /* 1080p */
            if (xfps) return 3;
            if (hfps) return 3;
            return 0;
        case 2: return 5;  /* 3K crop */
        case 3: return hfps ? 7 : 6;
        case 4: return 9;  /* full-res */
    }
    return 0;
}

static int cine_readout_pct_index(int fps_sel)
{
    if (fps_sel >= 5) return 3;
    if (fps_sel >= 4) return 2;
    return 0;
}

static const char * cine_field_label(int field)
{
    switch (field)
    {
        case CINE_FIELD_BEAST:  return "POWER MODE";
        case CINE_FIELD_RES:    return "RESOLUTION";
        case CINE_FIELD_ASPECT: return "ASPECT";
        case CINE_FIELD_DEPTH:  return "BIT DEPTH";
        case CINE_FIELD_FPS:    return "FRAME RATE";
        case CINE_FIELD_ANAM:   return "ANAMORPHIC";
    }
    return "?";
}

static const char * cine_field_value(int field)
{
    static char buf[32];
    switch (field)
    {
        case CINE_FIELD_BEAST:
            return cine_beast_labels[cine_beast];
        case CINE_FIELD_RES:
            return cine_res_labels[cine_res];
        case CINE_FIELD_ASPECT:
            return cine_aspect_labels[cine_aspect];
        case CINE_FIELD_DEPTH:
            return cine_depth_labels[cine_depth];
        case CINE_FIELD_FPS:
            snprintf(buf, sizeof(buf), "%s fps", cine_fps_labels[cine_fps]);
            return buf;
        case CINE_FIELD_ANAM:
            return cine_anam_labels[cine_anam];
    }
    return "?";
}

static void cine_clamp_selections(void)
{
    cine_beast  = COERCE(cine_beast, 0, COUNT(cine_beast_labels) - 1);
    cine_res    = COERCE(cine_res, 0, COUNT(cine_res_labels) - 1);
    cine_aspect = COERCE(cine_aspect, 0, COUNT(cine_aspect_labels) - 1);
    cine_depth  = COERCE(cine_depth, 0, COUNT(cine_depth_labels) - 1);
    cine_fps    = COERCE(cine_fps, 0, COUNT(cine_fps_labels) - 1);
    cine_anam   = COERCE(cine_anam, 0, COUNT(cine_anam_labels) - 1);
    cine_field  = COERCE(cine_field, 0, CINE_FIELD_ROWS - 1);
}

static int cine_adjust_field(int field, int delta)
{
    cine_clamp_selections();
    switch (field)
    {
        case CINE_FIELD_BEAST:
            cine_beast = MOD(cine_beast + delta, COUNT(cine_beast_labels));
            break;
        case CINE_FIELD_RES:
            cine_res = MOD(cine_res + delta, COUNT(cine_res_labels));
            break;
        case CINE_FIELD_ASPECT:
            cine_aspect = MOD(cine_aspect + delta, COUNT(cine_aspect_labels));
            break;
        case CINE_FIELD_DEPTH:
            cine_depth = MOD(cine_depth + delta, COUNT(cine_depth_labels));
            break;
        case CINE_FIELD_FPS:
            cine_fps = MOD(cine_fps + delta, COUNT(cine_fps_labels));
            break;
        case CINE_FIELD_ANAM:
            cine_anam = MOD(cine_anam + delta, COUNT(cine_anam_labels));
            break;
        default:
            return 0;
    }
    return 1;
}

static int cine_apply_settings(void)
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

    cine_clamp_selections();

    int crop, ro_pct, res_x, aspect, fmt, fps_i, preview_scale;

    if (cine_beast == 1) /* BEAST 4K25 — UHD crop, 10-bit, 25fps */
    {
        crop = 6;
        ro_pct = 0;
        res_x = 9;   /* 3520, closest to 3840 UHD window */
        aspect = 10; /* 16:9 */
        fmt = 2;     /* 10-bit */
        fps_i = 34;  /* 25 fps */
        preview_scale = 2; /* 50% preview */
        cine_res = 3; cine_aspect = 0; cine_depth = 2; cine_fps = 1;
    }
    else if (cine_beast == 2) /* BEAST 1080p120 — max crop path, 10-bit */
    {
        crop = 3;    /* 3x3 50/60 */
        ro_pct = 2;  /* 50% readout */
        res_x = 4;   /* 1920 */
        aspect = 10;
        fmt = 2;
        fps_i = 77;  /* 120 fps target — HW may clamp */
        preview_scale = 3; /* 25% preview */
        cine_res = 1; cine_aspect = 0; cine_depth = 2; cine_fps = 5;
        NotifyBox(3000, "Set Canon menu to 720p 50/60 for high FPS.");
    }
    else
    {
        crop = cine_crop_index(cine_res, cine_fps);
        ro_pct = cine_readout_pct_index(cine_fps);
        res_x = cine_res_x_index(cine_res_target_w[cine_res]);
        aspect = cine_aspect_idx[cine_aspect];
        fmt = cine_depth_fmt[cine_depth];
        fps_i = cine_fps_idx[cine_fps];
        preview_scale = (cine_fps >= 4) ? 3 : (cine_fps >= 2) ? 2 : 0;
    }

    NotifyBox(5000, "Applying cinema profile...");

    set_config_var("crop.preset", crop);
    msleep(900);

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

    /* anamorphic.preview: 0=OFF, 7=2:1 stretch for 2x lenses */
    set_config_var("anamorphic.preview", cine_anam ? 7 : 0);

    msleep(300);
    NotifyBoxHide();
    beep();

    cine_last_apply_ms = get_ms_clock();
    return 1;
}

static void cine_schedule_apply(void)
{
    if (!cine_auto_apply) return;
    cine_apply_pending = 1;
}

static void cine_draw_screen(void)
{
    bmp_fill(COLOR_ORANGE, 0, 0, 720, 480);

    bmp_printf(FONT(FONT_CANON, COLOR_WHITE, COLOR_ORANGE), 20, 10, "CINEMA OS RECORD");

    int sub_fnt = FONT(FONT_MED, COLOR_BLACK, COLOR_ORANGE);
    bmp_printf(sub_fnt, 20, 44, "Up/Dn row  L/R value  SET apply");

    int y0 = 72;
    int row_h = 54;

    for (int row = 0; row < CINE_FIELD_ROWS; row++)
    {
        int field = cine_field_order[row];
        int y = y0 + row * row_h;
        int selected = (row == cine_field);

        if (selected)
            bmp_fill(COLOR_WHITE, 10, y - 2, 700, row_h - 4);
        else
            bmp_fill(45, 10, y - 2, 700, row_h - 4);

        int label_fnt = FONT(FONT_MED, selected ? COLOR_ORANGE : COLOR_GRAY(55), selected ? COLOR_WHITE : 45);
        int value_fnt = FONT(FONT_LARGE, selected ? COLOR_ORANGE : COLOR_WHITE, selected ? COLOR_WHITE : 45);

        bmp_printf(label_fnt, 24, y + 2, "%s", cine_field_label(field));
        bmp_printf(value_fnt, 24, y + 22, "%s", cine_field_value(field));

        if (selected)
            bmp_printf(FONT(FONT_MED, COLOR_ORANGE, COLOR_WHITE), 620, y + 16, "< >");
    }

    int foot_y = 430;
    bmp_fill(COLOR_BLACK, 0, foot_y, 720, 50);
    if (cine_beast)
        bmp_printf(FONT(FONT_MED, COLOR_ORANGE, COLOR_BLACK), 16, foot_y + 8, "BEAST: %s", cine_beast_labels[cine_beast]);
    if (get_config_var("raw.video.enabled"))
        bmp_printf(FONT(FONT_MED, COLOR_GREEN1, COLOR_BLACK), 560, foot_y + 8, "ARMED");
}

static MENU_UPDATE_FUNC(cine_record_update)
{
    if (!entry->selected)
    {
        cine_record_active = 0;
        return;
    }

    cine_record_active = 1;
    cine_clamp_selections();

    if (cine_apply_pending && get_ms_clock() - cine_last_apply_ms > 400)
    {
        cine_apply_pending = 0;
        cine_apply_settings();
    }

    cine_draw_screen();
    info->custom_drawing = CUSTOM_DRAW_THIS_MENU;
}

static void cine_record_select(void * priv, int delta)
{
    (void)priv;
    if (!delta) return;
    int field = cine_field_order[cine_field];
    if (cine_adjust_field(field, delta))
    {
        cine_schedule_apply();
        menu_redraw();
    }
}

static int cine_record_keypress(unsigned int key)
{
    if (!cine_record_active || !gui_menu_shown())
        return 1;

    if (!is_menu_entry_selected("Movie", "Cinema Record"))
        return 1;

    switch (key)
    {
        case MODULE_KEY_JOY_UP:
        case MODULE_KEY_WHEEL_UP:
            cine_field = MOD(cine_field - 1, CINE_FIELD_ROWS);
            menu_redraw();
            return 0;

        case MODULE_KEY_JOY_DOWN:
        case MODULE_KEY_WHEEL_DOWN:
            cine_field = MOD(cine_field + 1, CINE_FIELD_ROWS);
            menu_redraw();
            return 0;

        case MODULE_KEY_JOY_LEFT:
        case MODULE_KEY_WHEEL_LEFT:
            cine_record_select(NULL, -1);
            return 0;

        case MODULE_KEY_JOY_RIGHT:
        case MODULE_KEY_WHEEL_RIGHT:
            cine_record_select(NULL, 1);
            return 0;

        case MODULE_KEY_PRESS_SET:
            cine_apply_settings();
            menu_redraw();
            return 0;
    }

    return 1;
}

static struct menu_entry cine_record_menu[] =
{
    {
        .name = "Cinema Record",
        .select = cine_record_select,
        .update = cine_record_update,
        .icon_type = IT_ACTION,
        .depends_on = DEP_LIVEVIEW | DEP_MOVIE_MODE,
        .help = "Sony-style scroll UI: resolution, aspect, bit depth, FPS.",
        .help2 = "Sets crop mode, MLV format and FPS override together.\n"
                  "5D3 hardware still limits max fps/resolution per mode.",
    },
};

void cine_record_init(void)
{
    menu_add("Movie", cine_record_menu, COUNT(cine_record_menu));
}
