/** \file
 * Cinema OS 2026 — standalone VRAM menu engine (BMP assets, crystal rows, two-column grid).
 */
#include "dryos.h"
#include "bmp.h"
#include "font.h"
#include "menu.h"
#include "cinema_os.h"
#include "cinema_gui_engine.h"
#include "cinema_ui_theme.h"
#include "cinema_menu_engine.h"
#include "cinema_debug.h"
#include "cinema_boot.h"

#define CME_COLS           2
#define CME_COL_W          348
#define CME_COL_GAP        16
#define CME_VISIBLE_ROWS   6

static int cme_count_visible(struct menu * m)
{
    int n = 0;
    for (struct menu_entry * e = m->children; e; e = e->next)
        if (menu_cinema_entry_visible(e))
            n++;
    return n;
}

static struct menu_entry * cme_visible_at(struct menu * m, int target)
{
    int vi = 0;
    for (struct menu_entry * e = m->children; e; e = e->next)
    {
        if (!menu_cinema_entry_visible(e))
            continue;
        if (vi == target)
            return e;
        vi++;
    }
    return 0;
}

static void cme_sync_scroll(struct menu * tab, int sel_idx, int visible_rows)
{
    int sel_row = sel_idx / CME_COLS;
    int scroll_row = tab->scroll_pos / CME_COLS;
    if (sel_row < scroll_row)
        tab->scroll_pos = sel_row * CME_COLS;
    if (sel_row >= scroll_row + visible_rows)
        tab->scroll_pos = (sel_row - visible_rows + 1) * CME_COLS;
}

static void cme_draw_help(const char * help, int warn_level)
{
    if (!help || !help[0])
        return;
    int c = warn_level == MENU_WARN_NOT_WORKING ? COLOR_ORANGE
        : warn_level == MENU_WARN_ADVICE ? COLOR_YELLOW
        : COLOR_GRAY(45);
    cinema_gui_draw_text_shadow(FONT_SMALL, 16, 398, help, c);
}

static void cme_draw_entry_row(
    struct menu * tab,
    struct menu_entry * entry,
    int col, int py, int accent, int only_selected)
{
    if (!entry)
        return;
    if (only_selected && !entry->selected)
        return;

    struct menu_display_info info;
    menu_cinema_fill_entry_info(tab, entry, &info);
    if (info.custom_drawing == CUSTOM_DRAW_THIS_MENU)
        return;

    int sel = entry->selected;
    int scale = sel ? 115 : 100;
    int row_h = CINE_ROW_H * scale / 100;
    int rx = 10 + col * (CME_COL_W + CME_COL_GAP);
    int ry = sel ? py - 4 : py;

    if (sel)
        cinema_gui_draw_crystal_row(rx, ry, CME_COL_W, row_h, accent, scale);

    int text_x = rx + 16;
    cinema_gui_draw_text_shadow(sel ? FONT_CANON : FONT_LARGE, text_x, ry + (sel ? 8 : 10),
        info.name, COLOR_WHITE);

    if (info.value[0])
        cinema_gui_draw_text_shadow(FONT_MED, text_x, ry + (sel ? 30 : 32),
            info.value, sel ? accent : COLOR_WHITE);

    if (entry->icon_type == IT_SUBMENU || entry->children)
        bmp_printf(FONT(FONT_MED, COLOR_WHITE, NO_BG_ERASE),
            rx + CME_COL_W - 24, ry + 18, ">");

    if (sel && menu_cinema_edit_mode() && entry->selected)
        menu_cinema_draw_pickbox(entry, rx + 8, ry + row_h + 6, accent);
}

static void cme_draw_tab_grid(struct menu * tab, int list_y)
{
    cinema_os_page_t page = cinema_os_active_page();
    int accent = cinema_os_page_color(page);
    int y0 = cinema_os_tab_bar_height();
    int body_h = 480 - y0 - 50;

    cinema_os_draw_page_background(page, y0, body_h + CINE_SUBHEADER_H);

    int total = cme_count_visible(tab);
    int visible_rows = CME_VISIBLE_ROWS;
    int sel_idx = 0;
    struct menu_entry * sel_entry = 0;
    int idx = 0;

    for (struct menu_entry * e = tab->children; e; e = e->next)
    {
        if (!menu_cinema_entry_visible(e))
            continue;
        if (e->selected)
        {
            sel_idx = idx;
            sel_entry = e;
        }
        idx++;
    }

    cme_sync_scroll(tab, sel_idx, visible_rows);
    int first_row = tab->scroll_pos / CME_COLS;
    int only_selected = menu_cinema_edit_mode();

    for (int vr = 0; vr < visible_rows; vr++)
    {
        int py = list_y + vr * CINE_ROW_H;
        for (int col = 0; col < CME_COLS; col++)
        {
            int target = (first_row + vr) * CME_COLS + col;
            struct menu_entry * entry = cme_visible_at(tab, target);
            if (!entry)
                continue;
            cme_draw_entry_row(tab, entry, col, py, accent, only_selected);
        }
    }

    int total_rows = (total + CME_COLS - 1) / CME_COLS;
    cine_ui_draw_scrollbar(708, list_y, visible_rows * CINE_ROW_H,
        total_rows, visible_rows, first_row, accent);

    if (sel_entry)
    {
        struct menu_display_info info;
        menu_cinema_fill_entry_info(tab, sel_entry, &info);
        cme_draw_help(info.help, info.warning_level);
        if (info.warning[0])
            cinema_gui_draw_text_shadow(FONT_SMALL, 16, 418, info.warning, COLOR_ORANGE);
    }
}

static void cme_draw_submenu_overlay(struct menu * submenu)
{
    int count = cme_count_visible(submenu);
    int row_h = CINE_ROW_H;
    int h = MIN(400, count * row_h + 56);
    int w = 680;
    int bx = (720 - w) / 2;
    int by = 48;
    int accent = cine_ui_menu_accent(submenu);

    cinema_gui_draw_page_bg(cinema_os_active_page(), 0, CINE_NAV_H, 720, 370);
    cine_ui_draw_veil_20(0, CINE_NAV_H, 720, 370);

    cine_ui_draw_panel_frame(bx, by, w, h, accent, submenu->name);

    int list_y = by + 52;
    int visible = MIN(count, 7);
    int sel_idx = 0;
    int idx = 0;
    struct menu_entry * sel_entry = 0;

    for (struct menu_entry * e = submenu->children; e; e = e->next)
    {
        if (!menu_cinema_entry_visible(e))
            continue;
        if (e->selected)
        {
            sel_idx = idx;
            sel_entry = e;
        }
        idx++;
    }

    if (sel_idx < submenu->scroll_pos)
        submenu->scroll_pos = sel_idx;
    if (sel_idx >= submenu->scroll_pos + visible)
        submenu->scroll_pos = sel_idx - visible + 1;

    int first = submenu->scroll_pos;
    int only_selected = menu_cinema_edit_mode();

    for (int vr = 0; vr < visible; vr++)
    {
        int py = list_y + vr * row_h;
        struct menu_entry * entry = cme_visible_at(submenu, first + vr);
        if (!entry)
            continue;

        if (only_selected && !entry->selected)
            continue;

        struct menu_display_info info;
        menu_cinema_fill_entry_info(submenu, entry, &info);

        int sel = entry->selected;
        int scale = sel ? 115 : 100;
        int rh = row_h * scale / 100;
        int rx = bx + 16;
        int ry = sel ? py - 3 : py;

        if (sel)
            cinema_gui_draw_crystal_row(rx, ry, w - 32, rh, accent, scale);

        cinema_gui_draw_text_shadow(sel ? FONT_CANON : FONT_LARGE,
            rx + 12, ry + (sel ? 8 : 10), info.name, COLOR_WHITE);

        if (info.value[0])
            cinema_gui_draw_text_shadow(FONT_MED, rx + 280, ry + (sel ? 10 : 12),
                info.value, sel ? accent : COLOR_WHITE);

        if (sel && menu_cinema_edit_mode())
            menu_cinema_draw_pickbox(entry, rx + 8, ry + rh + 4, accent);
    }

    if (count > visible)
        cine_ui_draw_scrollbar(bx + w - 12, list_y, visible * row_h,
            count, visible, first, accent);

    if (sel_entry)
    {
        struct menu_display_info info;
        menu_cinema_fill_entry_info(submenu, sel_entry, &info);
        cme_draw_help(info.help, info.warning_level);
    }
}

void cinema_menu_engine_draw(int y)
{
    cinema_os_draw_nav_bar(y);

    int list_y = y + cinema_os_tab_bar_height() + 40;

    if (menu_cinema_submenu_level())
    {
        struct menu * submenu = menu_cinema_get_submenu();
        if (submenu)
            cme_draw_submenu_overlay(submenu);
    }
    else if (cinema_os_uses_cinematic_canvas())
    {
        cinema_os_draw_cinematic_page(list_y);
    }
    else
    {
        struct menu * tab = menu_cinema_get_active_tab();
        if (tab)
            cme_draw_tab_grid(tab, list_y);
    }

    if (!menu_cinema_submenu_level())
        cinema_os_draw_status_footer();

    cine_debug_draw_overlay();
}
