/** \file
 * Cinema OS 2026 — five-page operational UI shell for 5D3.
 */
#include "dryos.h"
#include "bmp.h"
#include "menu.h"
#include "config.h"
#include "font.h"
#include "cinema_os.h"
#include "lens.h"
#include "fps.h"
#include "propvalues.h"
#include "property.h"
#include "picstyle.h"
#include "cinema_governor.h"
#include "cinema_write_engine.h"
#include "cinema_boot.h"
#include "cinema_panels.h"
#include "gui.h"

static CONFIG_INT("cinema.os", cinema_os, 1);
static CONFIG_INT("cinema.os.page", cinema_os_page, CINE_PAGE_CINEMATIC);

static int cine_row_sel = 0;
static int cine_row_scroll = 0;

static const char * page_labels[CINE_PAGE_COUNT] = {
    "SETTINGS", "PHOTO", "CINE", "ADD-ONS", "HACKS"
};

static const int page_icons[CINE_PAGE_COUNT] = {
    ICON_ML_PREFS, ICON_ML_SHOOT, ICON_ML_MOVIE, ICON_ML_MODULES, ICON_ML_DEBUG
};

static const int page_colors[CINE_PAGE_COUNT] = {
    CINE_COLOR_SETTINGS, CINE_COLOR_PHOTO, CINE_COLOR_CINEMA,
    CINE_COLOR_ADDONS, CINE_COLOR_HACKS
};

static const char * page_menu_names[CINE_PAGE_COUNT] = {
    "Prefs", "Shoot", "Movie", "Modules", "Debug"
};

static const int page_menu_icons[CINE_PAGE_COUNT] = {
    ICON_ML_PREFS, ICON_ML_SHOOT, ICON_ML_MOVIE, ICON_ML_MODULES, ICON_ML_DEBUG
};

enum {
    CINE_ROW_RES = 0,
    CINE_ROW_FPS,
    CINE_ROW_FORMAT,
    CINE_ROW_GAMMA,
    CINE_ROW_SHUTTER,
    CINE_ROW_APERTURE,
    CINE_ROW_ISO,
    CINE_ROW_WB,
    CINE_ROW_AUDIO,
    CINE_ROW_COUNT = 9
};

static const char * row_abbr[CINE_ROW_COUNT] = {
    "Res", "FPS", "Fmt", "Gam", "Sht", "Apt", "ISO", "WB", "Aud"
};

static const char * row_titles[CINE_ROW_COUNT] = {
    "RESOLUTION", "FRAME RATE", "CODEC/FORMAT", "GAMMA CURVE",
    "SHUTTER", "APERTURE", "ISO / GAIN", "WHITE BALANCE", "AUDIO MONITOR"
};

int cinema_os_enabled(void) { return cinema_os; }
int* cinema_os_enabled_var(void) { return &cinema_os; }

cinema_os_page_t cinema_os_active_page(void)
{
    return COERCE(cinema_os_page, 0, CINE_PAGE_COUNT - 1);
}

void cinema_os_set_page(cinema_os_page_t page)
{
    cinema_os_page = COERCE(page, 0, CINE_PAGE_COUNT - 1);
    cine_row_sel = 0;
    cine_row_scroll = 0;
}

void cinema_os_page_nav(int delta)
{
    int p = cinema_os_page + delta;
    if (p < 0) p = CINE_PAGE_COUNT - 1;
    if (p >= CINE_PAGE_COUNT) p = 0;
    cinema_os_set_page((cinema_os_page_t) p);
}

int cinema_os_page_color(cinema_os_page_t page)
{
    if (page < 0 || page >= CINE_PAGE_COUNT) return COLOR_WHITE;
    return page_colors[page];
}

const char * cinema_os_page_menu_name(cinema_os_page_t page)
{
    if (page < 0 || page >= CINE_PAGE_COUNT) return "Movie";
    return page_menu_names[page];
}

int cinema_os_page_menu_icon(cinema_os_page_t page)
{
    if (page < 0 || page >= CINE_PAGE_COUNT) return ICON_ML_MOVIE;
    return page_menu_icons[page];
}

int cinema_os_tab_bar_height(void) { return CINE_NAV_H; }
int cinema_os_entry_row_height(void) { return 52; }

int cinema_os_uses_cinematic_canvas(void)
{
    return cinema_os_enabled() && cinema_os_active_page() == CINE_PAGE_CINEMATIC;
}

int cinema_os_uses_pro_shell(void)
{
    return cinema_os_enabled();
}

void cinema_os_on_menu_open(void)
{
    cinema_boot_on_menu_open();
}

void cinema_os_draw_status_footer(void)
{
    if (!cinema_os_enabled()) return;

    int foot_y = 432;
    bmp_fill(COLOR_BLACK, 0, foot_y, 720, 48);
    bmp_printf(FONT(FONT_MED, COLOR_ORANGE, COLOR_BLACK), 12, foot_y + 4, "CINE AI ENHANCED");
    bmp_printf(FONT(FONT_SMALL, COLOR_GRAY(50), COLOR_BLACK), 12, foot_y + 24,
        "Wheel L/R pages  |  Up/Dn select  |  SET opens panel");

    if (cinema_os_uses_cinematic_canvas())
    {
        bmp_printf(FONT(FONT_SMALL, COLOR_WHITE, COLOR_BLACK), 280, foot_y + 4,
            "%s", cinema_write_speed_label());
        bmp_printf(FONT(FONT_SMALL, COLOR_WHITE, COLOR_BLACK), 280, foot_y + 24,
            "PROFILE: %s", cinema_write_profile_label());
        if (cinema_governor_fallback_active())
            bmp_printf(FONT(FONT_SMALL, COLOR_ORANGE, COLOR_BLACK), 520, foot_y + 4, "GOVERNOR ON");
        else if (cinema_write_engine_ready())
            bmp_printf(FONT(FONT_MED, COLOR_GREEN1, COLOR_BLACK), 640, foot_y + 10, "READY");
    }
}

/* ---- value formatters for CINEMATIC rows ---- */

static const int res_presets[] = {
    640, 960, 1280, 1600, 1920, 2240, 2560, 2880, 3072, 3520, 4096, 5796
};

static void cine_fmt_resolution(char * buf, int len)
{
    int idx = get_config_var("raw.res_x");
    int fine = get_config_var("raw.res_x_fine");
    int w = (idx >= 0 && idx < COUNT(res_presets)) ? res_presets[idx] + fine : 1920;
    int h = w * 9 / 16;

    if (w >= 3800)
        snprintf(buf, len, "4K UHD (%dx%d)", w, h);
    else if (w >= 2500)
        snprintf(buf, len, "2.7K (%dx%d)", w, h);
    else if (w >= 1800)
        snprintf(buf, len, "1080p (%dx%d)", w, h);
    else
        snprintf(buf, len, "%dx%d", w, h);
}

static void cine_fmt_fps(char * buf, int len)
{
    int fps = fps_get_current_x1000();
    if (fps <= 0) fps = video_mode_fps * 1000;
    int whole = fps / 1000;
    int frac = fps % 1000;
    if (frac == 976 || frac == 977) snprintf(buf, len, "23.976 fps");
    else if (frac == 0) snprintf(buf, len, "%d.000 fps", whole);
    else snprintf(buf, len, "%d.%03d fps", whole, frac);
}

static void cine_fmt_gamma(char * buf, int len)
{
    const char * ps = get_picstyle_name(lens_info.raw_picstyle);
    if (!ps || !ps[0]) ps = "Standard";
    snprintf(buf, len, "LOG (%s)", ps);
}

static void cine_fmt_shutter(char * buf, int len)
{
    int sr = get_current_shutter_reciprocal_x1000();
    int angle = sr ? (500000 / sr) : 180;
    snprintf(buf, len, "%d deg (%s)", angle, lens_format_shutter_reciprocal(sr, 1));
}

static void cine_fmt_aperture(char * buf, int len)
{
    snprintf(buf, len, "%s", lens_format_aperture(lens_info.raw_aperture));
}

static void cine_fmt_iso(char * buf, int len)
{
    snprintf(buf, len, "%d EI", raw2iso(lens_info.raw_iso));
}

static void cine_fmt_wb(char * buf, int len)
{
    if (lens_info.wb_mode == WB_KELVIN)
        snprintf(buf, len, "%dK", lens_info.kelvin);
    else if (lens_info.wb_mode == 1)
        snprintf(buf, len, "5600K (Daylight)");
    else if (lens_info.wb_mode == 0)
        snprintf(buf, len, "Auto WB");
    else
        snprintf(buf, len, "WB mode %d", lens_info.wb_mode);
}

static void cine_fmt_audio(char * buf, int len)
{
    snprintf(buf, len, "Ch 1 & 2");
}

static void cine_row_value(int row, char * buf, int len)
{
    switch (row)
    {
        case CINE_ROW_RES:     cine_fmt_resolution(buf, len); break;
        case CINE_ROW_FPS:     cine_fmt_fps(buf, len); break;
        case CINE_ROW_FORMAT:  snprintf(buf, len, "%s", cinema_governor_format_label()); break;
        case CINE_ROW_GAMMA:   cine_fmt_gamma(buf, len); break;
        case CINE_ROW_SHUTTER: cine_fmt_shutter(buf, len); break;
        case CINE_ROW_APERTURE:cine_fmt_aperture(buf, len); break;
        case CINE_ROW_ISO:     cine_fmt_iso(buf, len); break;
        case CINE_ROW_WB:      cine_fmt_wb(buf, len); break;
        case CINE_ROW_AUDIO:   cine_fmt_audio(buf, len); break;
        default:               snprintf(buf, len, "-"); break;
    }
}

/* ---- drawing primitives ---- */

static void cine_draw_box_icon(int x, int y, const char * abbr)
{
    bmp_fill(COLOR_TRANSPARENT_BLACK, x, y, 48, 34);
    bmp_draw_rect(COLOR_WHITE, x, y, 48, 34);
    bmp_printf(FONT(FONT_MED, COLOR_WHITE, NO_BG_ERASE), x + 6, y + 8, "%s", abbr);
}

static void cine_draw_chevron(int x, int y)
{
    bmp_printf(FONT(FONT_MED, COLOR_WHITE, NO_BG_ERASE), x, y, ">");
}

static void cine_draw_scrollbar(int y0, int h, int total, int visible, int scroll)
{
    if (total <= visible) return;
    int track_x = 708;
    int track_h = h - 8;
    int thumb_h = MAX(24, track_h * visible / total);
    int max_scroll = total - visible;
    int thumb_y = y0 + 4 + (track_h - thumb_h) * scroll / MAX(1, max_scroll);

    bmp_fill(COLOR_WHITE, track_x, y0 + 4, 4, track_h);
    bmp_fill(COLOR_GRAY(35), track_x, thumb_y, 4, thumb_h);
}

static void cine_draw_selection_pill(int x, int y, int w, int h)
{
    bmp_fill(COLOR_WHITE, x, y, w, h);
    bmp_fill(COLOR_GRAY(50), x + 3, y + 3, w - 6, h - 6);
    bmp_draw_rect(COLOR_WHITE, x, y, w, h);
    bmp_draw_rect(COLOR_GRAY(70), x + 1, y + 1, w - 2, h - 2);
}

/* ---- global nav bar ---- */

void cinema_os_draw_nav_bar(int y)
{
    int bar_h = CINE_NAV_H;
    bmp_fill(COLOR_BLACK, 0, y, 720, bar_h);

    int tile_w = 720 / CINE_PAGE_COUNT;
    cinema_os_page_t active = cinema_os_active_page();

    for (int i = 0; i < CINE_PAGE_COUNT; i++)
    {
        int x = i * tile_w;
        int color = page_colors[i];
        int sel = (i == (int) active);
        int fg = sel ? color : COLOR_GRAY(55);
        int icon = page_icons[i];

        bfnt_draw_char(icon, x + 10, y + 6, fg, NO_BG_ERASE);

        int lfnt = FONT(FONT_LARGE, fg, COLOR_BLACK);
        int lx = x + 42;
        if (lx + bmp_string_width(lfnt, page_labels[i]) > x + tile_w - 4)
            lfnt = FONT(FONT_MED, fg, COLOR_BLACK);
        bmp_printf(lfnt, lx, y + bar_h - font_large.height - 8, "%s", page_labels[i]);

        if (sel)
        {
            draw_line(x + 4, y + bar_h - 4, x + tile_w - 4, y + bar_h - 4, color);
            draw_line(x + 4, y + bar_h - 3, x + tile_w - 4, y + bar_h - 3, color);
            draw_line(x + 4, y + bar_h - 2, x + tile_w - 4, y + bar_h - 2, color);
        }
    }
}

void cinema_os_draw_page_background(cinema_os_page_t page, int y0, int h)
{
    int c = cinema_os_page_color(page);

    if (page == CINE_PAGE_CINEMATIC)
    {
        bmp_fill(c, 0, y0, 720, h);
        bmp_printf(FONT(FONT_LARGE, COLOR_WHITE, c),
            16, y0 + 8,
            "CINE MODE | RECORD SENSING / SETTINGS");
    }
    else
    {
        bmp_fill(COLOR_BLACK, 0, y0, 720, h);
        bmp_fill(c, 0, y0, 8, h);
        bmp_printf(FONT(FONT_MED, c, COLOR_BLACK), 16, y0 + 6, "%s", page_labels[page]);
    }
}

/* ---- CINEMATIC scrollable list ---- */

int cinema_os_draw_cinematic_page(int list_y)
{
    cinema_os_page_t page = CINE_PAGE_CINEMATIC;
    int bg = cinema_os_page_color(page);
    int y0 = CINE_NAV_H + CINE_SUBHEADER_H;
    int body_h = 480 - y0 - 50;

    cinema_os_draw_page_background(page, CINE_NAV_H, body_h + CINE_SUBHEADER_H);

    int row_y0 = list_y;
    int visible = CINE_VISIBLE_ROWS;

    if (cine_row_sel < cine_row_scroll)
        cine_row_scroll = cine_row_sel;
    if (cine_row_sel >= cine_row_scroll + visible)
        cine_row_scroll = cine_row_sel - visible + 1;

    for (int v = 0; v < visible; v++)
    {
        int row = cine_row_scroll + v;
        if (row >= CINE_ROW_COUNT) break;

        int y = row_y0 + v * CINE_ROW_H;
        int sel = (row == cine_row_sel);

        if (sel)
            cine_draw_selection_pill(8, y - 4, 704, CINE_ROW_H - 2);

        int fg = COLOR_WHITE;
        int row_bg = sel ? COLOR_GRAY(50) : bg;

        cine_draw_box_icon(18, y + 8, row_abbr[row]);

        char val[64];
        cine_row_value(row, val, sizeof(val));

        char line[96];
        snprintf(line, sizeof(line), "%s | %s", row_titles[row], val);
        bmp_printf(FONT(FONT_LARGE, fg, row_bg), 76, y + 14, "%s", line);

        cine_draw_chevron(686, y + 16);
    }

    cine_draw_scrollbar(row_y0, visible * CINE_ROW_H, CINE_ROW_COUNT, visible, cine_row_scroll);

    if (cinema_panel_is_open())
        cinema_panel_draw(row_y0, body_h);

    return 1;
}

static void cine_row_open(int row)
{
    if (row < 0 || row >= CINE_ROW_COUNT) return;
    cinema_panel_open(row);
}

int cinema_os_handle_key(unsigned int key)
{
    if (!cinema_os_enabled()) return 0;
    if (!cinema_os_uses_cinematic_canvas()) return 0;

    if (cinema_panel_is_open())
        return cinema_panel_handle_key(key);

    switch (key)
    {
        case BGMT_WHEEL_UP:
        case BGMT_PRESS_UP:
            cine_row_sel = MOD(cine_row_sel - 1, CINE_ROW_COUNT);
            return 1;

        case BGMT_WHEEL_DOWN:
        case BGMT_PRESS_DOWN:
            cine_row_sel = MOD(cine_row_sel + 1, CINE_ROW_COUNT);
            return 1;

        case BGMT_PRESS_SET:
            cine_row_open(cine_row_sel);
            return 1;

        default:
            return 0;
    }
}

/* ---- legacy ML tab integration ---- */

int cinema_os_tab_color(struct menu * tab)
{
    if (!tab) return CINE_COLOR_CINEMA;
    int icon = tab->icon ? tab->icon : tab->name[0];
    switch (icon)
    {
        case ICON_ML_MOVIE:    return CINE_COLOR_CINEMA;
        case ICON_ML_SHOOT:    return CINE_COLOR_PHOTO;
        case ICON_ML_EXPO:
        case ICON_ML_PREFS:    return CINE_COLOR_SETTINGS;
        case ICON_ML_MODULES:
        case ICON_ML_SCRIPT:   return CINE_COLOR_ADDONS;
        case ICON_ML_DEBUG:    return CINE_COLOR_HACKS;
        default:               return CINE_COLOR_CINEMA;
    }
}

void cinema_os_draw_tab_bar(struct menu * first_menu, int y)
{
    (void) first_menu;
    cinema_os_draw_nav_bar(y);
}

void cinema_os_draw_content_background(struct menu * tab)
{
    cinema_os_page_t page = cinema_os_active_page();
    int y0 = cinema_os_tab_bar_height();
    int h = 480 - y0 - 50;
    cinema_os_draw_page_background(page, y0, h);
    (void) tab;
}

void cinema_os_get_entry_colors(
    struct menu * tab,
    struct menu_entry * entry,
    struct menu_display_info * info,
    int selected_row,
    int * fg,
    int * bg,
    int * accent
)
{
    cinema_os_page_t page = cinema_os_active_page();
    int tab_c = cinema_os_page_color(page);
    int sel = entry && entry->selected;
    int warn = info && info->warning_level == MENU_WARN_NOT_WORKING;

    *accent = tab_c;

    if (page == CINE_PAGE_CINEMATIC || (tab && tab->icon == ICON_ML_MOVIE))
    {
        if (sel) { *bg = COLOR_WHITE; *fg = CINE_COLOR_CINEMA; }
        else if (warn) { *bg = 45; *fg = COLOR_GRAY(45); }
        else { *bg = CINE_COLOR_CINEMA; *fg = COLOR_WHITE; }
    }
    else
    {
        if (sel) { *bg = tab_c; *fg = COLOR_WHITE; }
        else if (warn) { *bg = 45; *fg = COLOR_GRAY(45); }
        else { *bg = COLOR_BLACK; *fg = COLOR_GRAY(60); }
    }

    (void) selected_row;
}
