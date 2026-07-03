/** \file
 * Cinema OS — reference-matched menu (single-column label | value rows).
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

static void cme_sync_scroll(struct menu * tab, int sel_idx, int visible)
{
    if (sel_idx < tab->scroll_pos)
        tab->scroll_pos = sel_idx;
    if (sel_idx >= tab->scroll_pos + visible)
        tab->scroll_pos = sel_idx - visible + 1;
}

static int cme_entry_icon(struct menu_entry * entry, struct menu_display_info * info)
{
    if (info && info->icon)
        return info->icon;
    if (!entry) return 0;
    switch (entry->icon_type)
    {
        case IT_SUBMENU: return ICON_ML_SUBMENU;
        default:         return 0;
    }
}

static void cme_draw_list(struct menu * tab, int list_y)
{
    cinema_os_page_t page = cinema_os_active_page();
    int accent = cinema_os_page_color(page);
    int total = cme_count_visible(tab);
    int visible = CINE_VISIBLE_ROWS;
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

    cme_sync_scroll(tab, sel_idx, visible);

    for (int vr = 0; vr < visible; vr++)
    {
        int py = list_y + vr * CINE_ROW_H;
        struct menu_entry * entry = cme_visible_at(tab, tab->scroll_pos + vr);
        if (!entry)
            continue;

        if (menu_cinema_edit_mode() && !entry->selected)
            continue;

        struct menu_display_info info;
        menu_cinema_fill_entry_info(tab, entry, &info);
        if (info.custom_drawing == CUSTOM_DRAW_THIS_MENU)
            return;

        cinema_gui_draw_menu_row(CINE_LIST_X, py, CINE_LIST_W, CINE_ROW_H,
            accent, entry->selected, cme_entry_icon(entry, &info), info.name, info.value);

        if (entry->selected && menu_cinema_edit_mode())
            menu_cinema_draw_pickbox(entry, CINE_LIST_X + 12, py + CINE_ROW_H, accent);
    }

    cinema_gui_draw_scrollbar(712, list_y, visible * CINE_ROW_H,
        total, visible, tab->scroll_pos, accent);

    if (sel_entry)
    {
        struct menu_display_info info;
        menu_cinema_fill_entry_info(tab, sel_entry, &info);
        if (info.help[0])
            cinema_gui_draw_text_shadow(FONT_SMALL, 16, 480 - CINE_FOOT_H - 22,
                info.help, COLOR_GRAY(45));
    }
}

static void cme_draw_submenu_overlay(struct menu * submenu)
{
    int count = cme_count_visible(submenu);
    int accent = cine_ui_menu_accent(submenu);
    int list_y = cinema_gui_list_top_y();
    int visible = MIN(count, CINE_VISIBLE_ROWS);
    int sel_idx = 0;
    int idx = 0;

    cine_ui_draw_veil_20(0, CINE_HDR_H, 720, 480 - CINE_HDR_H - CINE_FOOT_H);

    for (struct menu_entry * e = submenu->children; e; e = e->next)
    {
        if (!menu_cinema_entry_visible(e))
            continue;
        if (e->selected)
            sel_idx = idx;
        idx++;
    }

    cme_sync_scroll(submenu, sel_idx, visible);

    bmp_printf(FONT(FONT_CANON, COLOR_WHITE, NO_BG_ERASE), 24, CINE_HDR_H + CINE_NAV_H + 2, "%s", submenu->name);

    for (int vr = 0; vr < visible; vr++)
    {
        int py = list_y + vr * CINE_ROW_H;
        struct menu_entry * entry = cme_visible_at(submenu, submenu->scroll_pos + vr);
        if (!entry)
            continue;

        struct menu_display_info info;
        menu_cinema_fill_entry_info(submenu, entry, &info);

        cinema_gui_draw_menu_row(CINE_LIST_X, py, CINE_LIST_W, CINE_ROW_H,
            accent, entry->selected, cme_entry_icon(entry, &info), info.name, info.value);

        if (entry->selected && menu_cinema_edit_mode())
            menu_cinema_draw_pickbox(entry, CINE_LIST_X + 12, py + CINE_ROW_H, accent);
    }

    if (count > visible)
        cinema_gui_draw_scrollbar(712, list_y, visible * CINE_ROW_H,
            count, visible, submenu->scroll_pos, accent);
}

void cinema_menu_engine_draw(int y)
{
    (void) y;

    cinema_gui_draw_full_background(cinema_os_active_page());
    cinema_gui_draw_status_header();
    cinema_os_draw_nav_bar(0);

    int list_y = cinema_gui_list_top_y();

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
            cme_draw_list(tab, list_y);
    }

    if (!menu_cinema_submenu_level())
        cinema_gui_draw_status_footer();

    cine_debug_draw_overlay();
}
