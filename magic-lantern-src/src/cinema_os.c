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
#include "cinema_record_apply.h"
#include "cinema_ui_theme.h"
#include "cinema_gui_engine.h"
#include "cinema_thermal.h"
#include "beep.h"
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
    CINE_ROW_LV,
    CINE_ROW_DEPTH,
    CINE_ROW_FPS,
    CINE_ROW_FORMAT,
    CINE_ROW_GAMMA,
    CINE_ROW_SHUTTER,
    CINE_ROW_APERTURE,
    CINE_ROW_ISO,
    CINE_ROW_WB,
    CINE_ROW_PEAK,
    CINE_ROW_AUDIO,
    CINE_ROW_COUNT = 12
};

#define CINE_COLS            2
#define CINE_COL_W           348
#define CINE_COL_GAP         16

static const int row_icons[CINE_ROW_COUNT] = {
    CINE_ICON_RES, CINE_ICON_LV, CINE_ICON_SHUTTER, CINE_ICON_FPS, CINE_ICON_CODEC, CINE_ICON_GAMMA,
    CINE_ICON_SHUTTER, CINE_ICON_APERTURE, CINE_ICON_ISO, CINE_ICON_WB, CINE_ICON_FOCUS, CINE_ICON_AUDIO
};

static const char * row_titles[CINE_ROW_COUNT] = {
    "RESOLUTION", "LV PREVIEW %", "BIT DEPTH", "FRAME RATE", "CODEC/FORMAT", "GAMMA CURVE",
    "SHUTTER", "APERTURE", "ISO / GAIN", "WHITE BALANCE", "FOCUS PEAKING", "AUDIO MONITOR"
};

static const int lv_dial_steps[] = { 25, 50, 75, 100 };

static const char * row_abbr[CINE_ROW_COUNT] = {
    "Res", "LV", "Bit", "FPS", "Fmt", "Gam",
    "Shut", "Apt", "ISO", "WB", "Pk", "Aud"
};
static const int cine_panel_map[CINE_ROW_COUNT] = {
    0, -1, -1, 1, 2, 3, 4, 5, 6, 7, -1, 8
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

int cinema_os_page_nav_lr(int delta)
{
    if (!cinema_os_enabled()) return 0;
    cinema_os_page_nav(delta);
    return 1;
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
int cinema_os_entry_row_height(void) { return CINE_ROW_H; }

int cinema_os_uses_cinematic_canvas(void)
{
    return cinema_os_enabled() && cinema_os_active_page() == CINE_PAGE_CINEMATIC;
}

int cinema_os_uses_pro_shell(void)
{
    return cinema_os_skin_active();
}

int cinema_os_skin_active(void)
{
    return cinema_os_enabled();
}

void cinema_os_on_menu_open(void)
{
    cinema_boot_on_menu_open();
}

void cinema_os_draw_status_footer(void)
{
    if (!cinema_os_skin_active()) return;

    int foot_y = 432;
    int page_c = cinema_os_page_color(cinema_os_active_page());
    cine_ui_draw_hd_panel(0, foot_y, 720, 48, page_c);
    bmp_printf(FONT(FONT_MED, COLOR_WHITE, page_c), 16, foot_y + 6, "CINE AI ENHANCED");

    if (cinema_panel_is_open())
        bmp_printf(FONT(FONT_SMALL, page_c, COLOR_BLACK), 16, foot_y + 26,
            "L/R option   SET apply   MENU back");
    else if (cinema_os_uses_cinematic_canvas())
    {
        if (cine_row_sel == CINE_ROW_LV && !cinema_panel_is_open())
            bmp_printf(FONT(FONT_SMALL, page_c, COLOR_BLACK), 16, foot_y + 26,
                "L/R LV dial   Up/Dn row   SET panel");
        else if ((cine_row_sel == CINE_ROW_DEPTH || cine_row_sel == CINE_ROW_PEAK)
            && !cinema_panel_is_open())
            bmp_printf(FONT(FONT_SMALL, page_c, COLOR_BLACK), 16, foot_y + 26,
                "L/R adjust   Up/Dn row   SET apply");
        else
            bmp_printf(FONT(FONT_SMALL, page_c, COLOR_BLACK), 16, foot_y + 26,
                "L/R pages   Up/Dn row   SET panel");
    }
    else
        bmp_printf(FONT(FONT_SMALL, page_c, COLOR_BLACK), 16, foot_y + 26,
            "L/R pages   Up/Dn select   SET enter");

    if (cinema_os_uses_cinematic_canvas())
    {
        bmp_printf(FONT(FONT_SMALL, COLOR_WHITE, COLOR_BLACK), 200, foot_y + 4,
            "%s", cinema_write_speed_label());
        bmp_printf(FONT(FONT_SMALL, COLOR_WHITE, COLOR_BLACK), 200, foot_y + 24,
            "%s", cinema_write_profile_label());

        if (cinema_record_mlv_armed())
        {
            if (cinema_record_format_is_cinepack())
                bmp_printf(FONT(FONT_MED, COLOR_ORANGE, COLOR_BLACK), 500, foot_y + 4, "CSP ARMED");
            else
                bmp_printf(FONT(FONT_MED, COLOR_ORANGE, COLOR_BLACK), 520, foot_y + 4, "RAW ARMED");
        }
        else
            bmp_printf(FONT(FONT_MED, COLOR_LIGHT_BLUE, COLOR_BLACK), 520, foot_y + 4, "MOV MODE");

        if (cinema_governor_fallback_active())
            bmp_printf(FONT(FONT_SMALL, COLOR_ORANGE, COLOR_BLACK), 640, foot_y + 4, "GOV");
        else if (cinema_write_engine_ready())
            bmp_printf(FONT(FONT_MED, COLOR_GREEN1, COLOR_BLACK), 640, foot_y + 10, "READY");

        if (cinema_thermal_warn_active())
        {
            int tc = cinema_thermal_celsius();
            bmp_printf(FONT(FONT_SMALL, COLOR_RED, COLOR_BLACK), 560, foot_y + 26, "%dC", tc);
        }
    }
}

/* ---- value formatters for CINEMATIC rows ---- */

static void cine_fmt_resolution(char * buf, int len)
{
    snprintf(buf, len, "%s", cinema_record_resolution_label());
}

static void cine_fmt_fps(char * buf, int len)
{
    snprintf(buf, len, "%s", cinema_record_fps_label());
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

static void cine_fmt_lv(char * buf, int len)
{
    snprintf(buf, len, "%s", cinema_record_lv_label());
}

static void cine_fmt_depth(char * buf, int len)
{
    snprintf(buf, len, "%s", cinema_record_bpp_label());
}

static void cine_fmt_peak(char * buf, int len)
{
    snprintf(buf, len, "%s", cinema_record_peaking_on() ? "ON" : "OFF");
}

static void cine_row_value(int row, char * buf, int len)
{
    switch (row)
    {
        case CINE_ROW_RES:     cine_fmt_resolution(buf, len); break;
        case CINE_ROW_LV:      cine_fmt_lv(buf, len); break;
        case CINE_ROW_DEPTH:   cine_fmt_depth(buf, len); break;
        case CINE_ROW_FPS:     cine_fmt_fps(buf, len); break;
        case CINE_ROW_FORMAT:
            snprintf(buf, len, "%s", cinema_record_codec_display_label());
            break;
        case CINE_ROW_GAMMA:   cine_fmt_gamma(buf, len); break;
        case CINE_ROW_SHUTTER: cine_fmt_shutter(buf, len); break;
        case CINE_ROW_APERTURE:cine_fmt_aperture(buf, len); break;
        case CINE_ROW_ISO:     cine_fmt_iso(buf, len); break;
        case CINE_ROW_WB:      cine_fmt_wb(buf, len); break;
        case CINE_ROW_PEAK:    cine_fmt_peak(buf, len); break;
        case CINE_ROW_AUDIO:   cine_fmt_audio(buf, len); break;
        default:               snprintf(buf, len, "-"); break;
    }
}

static void cine_draw_lv_dial(int x, int y, int pct, int accent, int sel)
{
    int bar_w = 120;
    int bar_h = 14;
    int fg = sel ? COLOR_WHITE : accent;
    int bg = sel ? accent : COLOR_GRAY(30);
    int fill_w = bar_w * COERCE(pct, 25, 100) / 100;

    bmp_fill(bg, x, y, bar_w, bar_h);
    bmp_fill(fg, x, y, fill_w, bar_h);
    bmp_draw_rect(COLOR_WHITE, x, y, bar_w, bar_h);

    bmp_printf(FONT(FONT_MED, fg, sel ? accent : COLOR_BLACK),
        x + bar_w + 8, y - 2, "%d%%", pct);
}

static void cine_lv_step(int delta)
{
    if (ABS(delta) > 1)
    {
        cinema_record_set_lv_rec_fps(cinema_record_lv_rec_fps() == 12 ? 0 : 12);
        beep();
        return;
    }
    int cur = cinema_record_lv_pct();
    int best = 0;
    int best_d = 1000;
    for (int i = 0; i < COUNT(lv_dial_steps); i++)
    {
        int d = ABS(lv_dial_steps[i] - cur);
        if (d < best_d) { best_d = d; best = i; }
    }
    best += delta;
    if (best < 0) best = 0;
    if (best >= COUNT(lv_dial_steps)) best = COUNT(lv_dial_steps) - 1;
    cinema_record_set_lv_pct(lv_dial_steps[best]);
    beep();
}

static void cine_depth_step(int delta)
{
    int n = 4;
    int cur = cinema_record_bpp_idx();
    cur = MOD(cur + delta, n);
    cinema_record_set_bpp_idx(cur);
    beep();
}

static void cine_peak_toggle(void)
{
    cinema_record_set_peaking(!cinema_record_peaking_on());
    beep();
}

static int cine_row_is_dial(int row)
{
    return row == CINE_ROW_LV || row == CINE_ROW_DEPTH || row == CINE_ROW_PEAK;
}

static void cine_draw_glass_resolution_dropdown(int x, int y, int bg)
{
    static const char * opts[] = {
        "720p", "1080p", "2.7K", "4K UHD", "6K", "BEAST 4K25", "BEAST 1080p120"
    };
    int cols = 3;
    int cell_w = 210;
    int cell_h = 36;
    int panel_w = cols * cell_w + 24;
    int rows = (COUNT(opts) + cols - 1) / cols;
    int panel_h = rows * cell_h + 20;

    cine_ui_draw_glass_panel(x, y, panel_w, panel_h);
    bmp_printf(FONT(FONT_SMALL, COLOR_WHITE, COLOR_GRAY(50)), x + 12, y + 6, "RESOLUTION");

    for (int i = 0; i < COUNT(opts); i++)
    {
        int col = i % cols;
        int row = i / cols;
        int cx = x + 12 + col * cell_w;
        int cy = y + 22 + row * cell_h;
        bmp_printf(FONT(FONT_MED, COLOR_WHITE, COLOR_GRAY(45)), cx, cy, "%s", opts[i]);
    }
    (void) bg;
}

/* ---- drawing primitives ---- */

static void cine_draw_box_icon(int x, int y, int icon, int accent)
{
    bmp_fill(accent, x, y, 48, 40);
    bmp_draw_rect(COLOR_WHITE, x, y, 48, 40);
    bfnt_draw_char(icon, x + 8, y + 4, COLOR_WHITE, accent);
}

static void cine_draw_chevron(int x, int y)
{
    bmp_printf(FONT(FONT_MED, COLOR_WHITE, NO_BG_ERASE), x, y, ">");
}

static void cine_draw_scrollbar(int y0, int h, int total, int visible, int scroll, int accent)
{
    if (total <= visible) return;
    int track_x = 708;
    int track_h = h - 8;
    int thumb_h = MAX(24, track_h * visible / total);
    int max_scroll = total - visible;
    int thumb_y = y0 + 4 + (track_h - thumb_h) * scroll / MAX(1, max_scroll);

    bmp_fill(accent, track_x, y0 + 4, 4, track_h);
    bmp_fill(COLOR_WHITE, track_x, thumb_y, 4, thumb_h);
}

/* ---- global nav bar ---- */

void cinema_os_draw_nav_bar(int y)
{
    cinema_gui_draw_nav_bar(y, CINE_NAV_H, (int) cinema_os_active_page(),
        page_colors, page_labels, page_icons, CINE_PAGE_COUNT);
}

void cinema_os_draw_page_background(cinema_os_page_t page, int y0, int h)
{
    int c = cinema_os_page_color(page);
    cinema_gui_draw_page_bg(page, 0, y0, 720, h);

    if (page == CINE_PAGE_CINEMATIC)
        cinema_gui_draw_text_shadow(FONT_LARGE, 28, y0 + 18,
            "CINE MODE | RECORD SENSING / SETTINGS", COLOR_WHITE);
    else
        cinema_gui_draw_text_shadow(FONT_MED, 28, y0 + 18, page_labels[page], COLOR_WHITE);
    (void) c;
}

/* ---- CINEMATIC scrollable list ---- */

int cinema_os_draw_cinematic_page(int list_y)
{
    cinema_os_page_t page = CINE_PAGE_CINEMATIC;
    int accent = cinema_os_page_color(page);
    int y0 = CINE_NAV_H + CINE_SUBHEADER_H;
    int body_h = 480 - y0 - 50;
    int visible_rows = CINE_VISIBLE_ROWS;

    cinema_os_draw_page_background(page, CINE_NAV_H, body_h + CINE_SUBHEADER_H);

    int row_y0 = list_y;
    int first_row = cine_row_scroll / CINE_COLS;

    if (cine_row_sel / CINE_COLS < first_row)
        cine_row_scroll = (cine_row_sel / CINE_COLS) * CINE_COLS;
    if (cine_row_sel / CINE_COLS >= first_row + visible_rows)
        cine_row_scroll = ((cine_row_sel / CINE_COLS) - visible_rows + 1) * CINE_COLS;

    first_row = cine_row_scroll / CINE_COLS;

    for (int vr = 0; vr < visible_rows; vr++)
    {
        int py = row_y0 + vr * CINE_ROW_H;

        for (int col = 0; col < CINE_COLS; col++)
        {
            int row = (first_row + vr) * CINE_COLS + col;
            if (row >= CINE_ROW_COUNT) continue;

            int sel = (row == cine_row_sel);
            int scale = sel ? 115 : 100;
            int row_h = CINE_ROW_H * scale / 100;
            int rx = 10 + col * (CINE_COL_W + CINE_COL_GAP);
            int ry = sel ? py - 4 : py;
            int rw = CINE_COL_W;

            if (sel)
                cinema_gui_draw_crystal_row(rx, ry, rw, row_h, accent, scale);
            else
                bmp_fill(accent, rx, py, rw, CINE_ROW_H - 4);

            int icon_y = sel ? ry + 10 : py + 6;
            int text_x = rx + (sel ? 64 : 56);
            int row_bg = sel ? COLOR_BLACK : accent;

            cine_ui_draw_abbr_icon(rx + (sel ? 12 : 8), icon_y, row_abbr[row], accent);

            char val[64];
            cine_row_value(row, val, sizeof(val));

            cinema_gui_draw_text_shadow(sel ? FONT_CANON : FONT_LARGE, text_x, ry + (sel ? 6 : 8),
                row_titles[row], COLOR_WHITE);
            cinema_gui_draw_text_shadow(FONT_MED, text_x, ry + (sel ? 28 : 30),
                val, sel ? accent : COLOR_WHITE);

            if (!cine_row_is_dial(row))
                bmp_printf(FONT(FONT_MED, COLOR_WHITE, NO_BG_ERASE),
                    rx + rw - 24, ry + (sel ? 18 : 14), ">");

            if (sel && row == CINE_ROW_RES && !cinema_panel_is_open())
                cine_draw_glass_resolution_dropdown(rx, ry + row_h + 4, accent);

            (void) row_bg;
        }
    }

    cine_ui_draw_scrollbar(708, row_y0, visible_rows * CINE_ROW_H,
        (CINE_ROW_COUNT + CINE_COLS - 1) / CINE_COLS, visible_rows,
        first_row, COLOR_WHITE);

    if (cinema_panel_is_open())
        cinema_panel_draw(row_y0, body_h);

    return 1;
}

static void cine_row_open(int row)
{
    if (row < 0 || row >= CINE_ROW_COUNT) return;
    if (cine_panel_map[row] < 0)
    {
        if (row == CINE_ROW_DEPTH || row == CINE_ROW_PEAK)
            cinema_record_apply_full();
        return;
    }
    cinema_panel_open(cine_panel_map[row]);
}

int cinema_os_handle_lr_key(int delta)
{
    if (!cinema_os_enabled() || !cinema_os_uses_cinematic_canvas()) return 0;
    if (cinema_panel_is_open()) return 0;

    switch (cine_row_sel)
    {
        case CINE_ROW_LV:
            if (delta > 0 || delta < 0)
                cine_lv_step(delta);
            return 1;
        case CINE_ROW_DEPTH:
            cine_depth_step(delta);
            return 1;
        case CINE_ROW_PEAK:
            cine_peak_toggle();
            return 1;
        default:
            if (delta > 0 || delta < 0)
            {
                cine_row_sel = MOD(cine_row_sel + delta, CINE_ROW_COUNT);
                return 1;
            }
            return 0;
    }
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
            cine_row_sel = MOD(cine_row_sel - CINE_COLS, CINE_ROW_COUNT);
            return 1;

        case BGMT_WHEEL_DOWN:
        case BGMT_PRESS_DOWN:
            cine_row_sel = MOD(cine_row_sel + CINE_COLS, CINE_ROW_COUNT);
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
    *fg = COLOR_WHITE;
    *bg = tab_c;

    if (sel && warn) { *bg = COLOR_ORANGE; *fg = COLOR_BLACK; }
    else if (warn) { *bg = COLOR_ORANGE; *fg = COLOR_BLACK; }

    (void) selected_row;
    (void) tab;
}
