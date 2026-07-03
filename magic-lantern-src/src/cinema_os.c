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


static const int row_icons[CINE_ROW_COUNT] = {
    CINE_ICON_RES, CINE_ICON_LV, CINE_ICON_SHUTTER, CINE_ICON_FPS, CINE_ICON_CODEC, CINE_ICON_GAMMA,
    CINE_ICON_SHUTTER, CINE_ICON_APERTURE, CINE_ICON_ISO, CINE_ICON_WB, CINE_ICON_FOCUS, CINE_ICON_AUDIO
};

static const char * row_titles[CINE_ROW_COUNT] = {
    "Resolution", "LV PREVIEW %", "BIT DEPTH", "FRAME RATE", "CODEC", "GAMMA",
    "SHUTTER", "APERTURE", "ISO / GAIN", "WHITE BALANCE", "FOCUS PEAKING", "AUDIO"
};

static const int lv_dial_steps[] = { 25, 50, 75, 100 };

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

int cinema_os_tab_bar_height(void) { return CINE_HDR_H + CINE_NAV_H; }
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
    cinema_gui_draw_status_footer();
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
    cinema_gui_draw_nav_bar(y + CINE_HDR_H, CINE_NAV_H, (int) cinema_os_active_page(),
        page_colors, page_labels, page_icons, CINE_PAGE_COUNT);
}

void cinema_os_draw_page_background(cinema_os_page_t page, int y0, int h)
{
    cinema_gui_draw_page_bg(page, 0, y0, 720, h);
    (void) page;
}

/* ---- CINEMATIC scrollable list ---- */

int cinema_os_draw_cinematic_page(int list_y)
{
    cinema_os_page_t page = CINE_PAGE_CINEMATIC;
    int accent = cinema_os_page_color(page);
    int visible = CINE_VISIBLE_ROWS;
    int list_h = visible * CINE_ROW_H;

    if (cine_row_sel < cine_row_scroll)
        cine_row_scroll = cine_row_sel;
    if (cine_row_sel >= cine_row_scroll + visible)
        cine_row_scroll = cine_row_sel - visible + 1;

    for (int vr = 0; vr < visible; vr++)
    {
        int row = cine_row_scroll + vr;
        if (row >= CINE_ROW_COUNT) break;

        int py = list_y + vr * CINE_ROW_H;
        int sel = (row == cine_row_sel);
        char val[64];
        cine_row_value(row, val, sizeof(val));

        cinema_gui_draw_menu_row(CINE_LIST_X, py, CINE_LIST_W, CINE_ROW_H,
            accent, sel, row_icons[row], row_titles[row], val);
    }

    cinema_gui_draw_scrollbar(712, list_y, list_h, CINE_ROW_COUNT, visible,
        cine_row_scroll, accent);

    if (cinema_panel_is_open())
    {
        int body_h = 480 - cinema_gui_list_top_y() - CINE_FOOT_H;
        cinema_panel_draw(list_y, body_h);
    }

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
    *fg = COLOR_WHITE;
    *bg = tab_c;

    if (sel && warn) { *bg = COLOR_ORANGE; *fg = COLOR_BLACK; }
    else if (warn) { *bg = COLOR_ORANGE; *fg = COLOR_BLACK; }

    (void) selected_row;
    (void) tab;
}
