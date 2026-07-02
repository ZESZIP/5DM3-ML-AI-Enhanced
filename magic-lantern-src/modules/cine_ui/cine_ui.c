/* Cinema UI: color-coded menus, LiveView performance dashboard (2026) */

#include <module.h>
#include <dryos.h>
#include <bmp.h>
#include <menu.h>
#include <config.h>
#include <lvinfo.h>
#include <property.h>
#include <propvalues.h>
#include <fps.h>
#include <ml-cbr.h>

extern int* cinema_os_enabled_var(void);
extern int cinema_write_speed_centi_mbs(void);

extern int ml_started;
extern void cine_record_init(void);
extern unsigned int cine_record_keypress(unsigned int key);

static CONFIG_INT("cine.dashboard", cinema_dashboard, 1);
static CONFIG_INT("cine.lite.lv", cinema_lite_lv, 0);

static const int pct_values[] = {100, 75, 50, 25};

static int pct_from_index(int index)
{
    if (index < 0 || index >= COUNT(pct_values))
        return 100;
    return pct_values[index];
}

static MENU_UPDATE_FUNC(cine_os_update)
{
    MENU_SET_VALUE(cinema_os_enabled_var() && *cinema_os_enabled_var() ? "ON" : "OFF");
    MENU_SET_HELP("2026 full-screen Delete menu: colored tabs, orange CINE workspace.");
}

static MENU_UPDATE_FUNC(cine_colors_update)
{
    MENU_SET_VALUE(*(menu_cine_colors_var()) ? "ON" : "OFF");
    MENU_SET_HELP("Color-code menu tabs and selection bars by category.");
}

static MENU_UPDATE_FUNC(cine_lite_lv_update)
{
    MENU_SET_VALUE(cinema_lite_lv ? "ON" : "OFF");
    MENU_SET_HELP("Hide non-essential dashboard items while recording.");
}

static struct menu_entry cine_ui_menu[] =
{
    {
        .name = "Cinema UI",
        .select = menu_open_submenu,
        .help = "2026 cinema UX: colors, dashboard, performance tweaks.",
        .children = (struct menu_entry[]) {
            {
                .name = "Cinema OS shell",
                .priv = NULL,
                .max = 1,
                .update = cine_os_update,
                .choices = CHOICES("OFF", "ON"),
                .icon_type = IT_BOOL,
                .help2 = "Delete menu becomes Cinema OS 2026:\n"
                          "big colored tabs, orange Movie workspace,\n"
                          "bold white text on CINE tab.",
            },
            {
                .name = "Color-coded menus",
                .priv = NULL,
                .max = 1,
                .update = cine_colors_update,
                .choices = CHOICES("OFF", "ON"),
                .icon_type = IT_BOOL,
                .help2 = "Orange=Movie, Green=Shoot, Yellow=Expo, Cyan=Focus,\n"
                          "Blue=Display, Magenta=Audio. Nothing hidden.",
            },
            {
                .name = "Cinema dashboard",
                .priv = &cinema_dashboard,
                .max = 1,
                .choices = CHOICES("OFF", "ON"),
                .icon_type = IT_BOOL,
                .help = "Show FPS, crop %, preview % and power override on LV bars.",
            },
            {
                .name = "Lite LV while recording",
                .priv = &cinema_lite_lv,
                .max = 1,
                .update = cine_lite_lv_update,
                .choices = CHOICES("OFF", "ON"),
                .icon_type = IT_BOOL,
                .help2 = "When ON, cinema dashboard hides FPS/crop/preview\n"
                          "tags during recording to reduce overlay work.",
            },
            {
                .name = "Color legend",
                .select = menu_open_submenu,
                .help = "Where to find performance controls (nothing moved or removed).",
                .children = (struct menu_entry[]) {
                    {
                        .name = "Cinema Record UI",
                        .help = "Movie > Cinema Record",
                        .help2 = "Sony-style full-screen scroll: resolution, aspect,\n"
                                  "bit depth, FPS and anamorphic in one place.",
                    },
                    {
                        .name = "Recording readout %",
                        .help = "Movie > Crop mode > Recording readout %",
                        .help2 = "Crops sensor lines for recording (MLV file). Orange tab.",
                    },
                    {
                        .name = "Preview scale %",
                        .help = "Movie > mlv_lite > Preview scale %",
                        .help2 = "Preview-only downscale; MLV stays full resolution.",
                    },
                    {
                        .name = "FPS override",
                        .help = "Movie > FPS override",
                        .help2 = "Up to 120 fps where hardware allows (5D3 clamps).",
                    },
                    {
                        .name = "Never power off",
                        .help = "Prefs > Powersave in LiveView",
                        .help2 = "Override Canon auto shutoff (use with care).",
                    },
                    MENU_EOL,
                },
            },
            MENU_EOL,
        },
    },
};

static LVINFO_UPDATE_FUNC(cine_cf_update)
{
    LVINFO_BUFFER(16);
    if (!cinema_dashboard) { item->width = 0; return; }
    if (!lv) { item->width = 0; return; }

    int s = cinema_write_speed_centi_mbs();
    if (s <= 0) { item->width = 0; return; }

    snprintf(buffer, sizeof(buffer), "%d.%dMB", s / 100, (s % 100) / 10);
    item->color_fg = COLOR_YELLOW;
    item->color_bg = COLOR_BLACK;
    item->priority = 5;
}

static LVINFO_UPDATE_FUNC(cine_brand_update)
{
    LVINFO_BUFFER(16);
    if (!lv) { item->width = 0; return; }
    snprintf(buffer, sizeof(buffer), "AI-CINEMA");
    item->color_fg = COLOR_ORANGE;
    item->color_bg = COLOR_BLACK;
    item->priority = 11;
}

static LVINFO_UPDATE_FUNC(cine_fps_update)
{
    LVINFO_BUFFER(16);
    if (!cinema_dashboard) { item->width = 0; return; }
    if (cinema_lite_lv && RECORDING) { item->width = 0; return; }
    if (!lv) { item->width = 0; return; }

    int fps = fps_get_current_x1000();
    if (fps <= 0) { item->width = 0; return; }

    snprintf(buffer, sizeof(buffer), "%d.%d", fps / 1000, (fps % 1000) / 100);
    item->color_fg = COLOR_CYAN;
    item->color_bg = COLOR_BLACK;
    item->priority = 8;
}

static LVINFO_UPDATE_FUNC(cine_readout_update)
{
    LVINFO_BUFFER(16);
    if (!cinema_dashboard) { item->width = 0; return; }
    if (cinema_lite_lv && RECORDING) { item->width = 0; return; }
    if (!lv) { item->width = 0; return; }

    int pct = pct_from_index(get_config_var("crop.lv_res_pct"));
    if (pct >= 100) { item->width = 0; return; }

    snprintf(buffer, sizeof(buffer), "RO:%d%%", pct);
    item->color_fg = COLOR_ORANGE;
    item->color_bg = COLOR_BLACK;
    item->priority = 7;
}

static LVINFO_UPDATE_FUNC(cine_preview_update)
{
    LVINFO_BUFFER(16);
    if (!cinema_dashboard) { item->width = 0; return; }
    if (cinema_lite_lv && RECORDING) { item->width = 0; return; }
    if (!lv) { item->width = 0; return; }

    int pct = pct_from_index(get_config_var("raw.preview.lv_scale"));
    if (pct >= 100) { item->width = 0; return; }

    snprintf(buffer, sizeof(buffer), "PV:%d%%", pct);
    item->color_fg = COLOR_LIGHT_BLUE;
    item->color_bg = COLOR_BLACK;
    item->priority = 6;
}

static LVINFO_UPDATE_FUNC(cine_power_update)
{
    LVINFO_BUFFER(8);
    if (!cinema_dashboard) { item->width = 0; return; }

    if (!get_config_var("idle.never.poweroff")) { item->width = 0; return; }

    snprintf(buffer, sizeof(buffer), "PWR!");
    item->color_fg = COLOR_RED;
    item->color_bg = COLOR_BLACK;
    item->priority = 9;
}

static LVINFO_UPDATE_FUNC(cine_rec_update)
{
    LVINFO_BUFFER(8);
    if (!cinema_dashboard) { item->width = 0; return; }
    if (!RECORDING) { item->width = 0; return; }

    snprintf(buffer, sizeof(buffer), "REC");
    item->color_fg = COLOR_RED;
    item->color_bg = COLOR_BLACK;
    item->priority = 10;
}

static struct lvinfo_item cine_lv_items[] =
{
    {
        .name = "Cine CF speed",
        .which_bar = LV_PREFER_TOP_BAR,
        .update = cine_cf_update,
        .preferred_position = -10,
        .priority = 5,
    },
    {
        .name = "AI Cinema brand",
        .which_bar = LV_PREFER_TOP_BAR,
        .update = cine_brand_update,
        .preferred_position = -50,
        .priority = 11,
    },
    {
        .name = "Cine FPS",
        .which_bar = LV_PREFER_TOP_BAR,
        .update = cine_fps_update,
        .preferred_position = -40,
        .priority = 8,
    },
    {
        .name = "Cine readout",
        .which_bar = LV_PREFER_TOP_BAR,
        .update = cine_readout_update,
        .preferred_position = -30,
        .priority = 7,
    },
    {
        .name = "Cine preview",
        .which_bar = LV_PREFER_TOP_BAR,
        .update = cine_preview_update,
        .preferred_position = -20,
        .priority = 6,
    },
    {
        .name = "Cine power",
        .which_bar = LV_PREFER_TOP_BAR,
        .update = cine_power_update,
        .preferred_position = 40,
        .priority = 9,
    },
    {
        .name = "Cine rec",
        .which_bar = LV_PREFER_TOP_BAR,
        .update = cine_rec_update,
        .preferred_position = 30,
        .priority = 10,
    },
};

static int cine_boot_msg_done = 0;

static unsigned int cine_boot_notify_cbr(unsigned int unused)
{
    (void)unused;
    if (cine_boot_msg_done || !ml_started) return 1;
    if (!lv) return 1;

    cine_boot_msg_done = 1;
    NotifyBox(10000,
        "CINEMA OS 2026\n"
        "\n"
        "Delete = CINE page (chunky UI)\n"
        "Write engine arms hacks + CF test\n"
        "\n"
        "Ready to record MLV"
    );
    return 1;
}

static unsigned int cine_ui_init(void)
{
    cine_ui_menu[0].children[0].priv = cinema_os_enabled_var();
    cine_ui_menu[0].children[1].priv = menu_cine_colors_var();
    menu_add("Display", cine_ui_menu, COUNT(cine_ui_menu));
    cine_record_init();
    lvinfo_add_items(cine_lv_items, COUNT(cine_lv_items));
    return 0;
}

static unsigned int cine_ui_deinit(void)
{
    return 0;
}

MODULE_INFO_START()
    MODULE_INIT(cine_ui_init)
    MODULE_DEINIT(cine_ui_deinit)
MODULE_INFO_END()

MODULE_CONFIGS_START()
    MODULE_CONFIG(cinema_dashboard)
    MODULE_CONFIG(cinema_lite_lv)
    MODULE_CONFIG(cine_res)
    MODULE_CONFIG(cine_aspect)
    MODULE_CONFIG(cine_depth)
    MODULE_CONFIG(cine_fps)
    MODULE_CONFIG(cine_anam)
    MODULE_CONFIG(cine_beast)
    MODULE_CONFIG(cine_auto_apply)
MODULE_CONFIGS_END()

MODULE_PROPHANDLERS_START()
MODULE_PROPHANDLERS_END()

MODULE_CBRS_START()
    MODULE_CBR(CBR_KEYPRESS, cine_record_keypress, 0)
    MODULE_CBR(CBR_SECONDS_CLOCK, cine_boot_notify_cbr, 0)
MODULE_CBRS_END()
