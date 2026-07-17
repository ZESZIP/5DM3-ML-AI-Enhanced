/*
 * Cinema OS menu shell — layout matches assets/cine_menu_preview.png
 *
 * Tabs: SETTINGS | PHOTO | CINE | ADD-ONS | HACKS
 * CINE rows: LUT, FRAME RATE, Resolution, CODEC, AUDIO, ASPECT RATIO
 *
 * This module is written against Magic Lantern / DryOS BMP APIs.
 * On host builds, compile with -DCINEMA_UI_HOST_STUB to exercise strings only.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef CINEMA_UI_HOST_STUB
#define bmp_printf(font, x, y, fmt, ...) printf("[%d,%d] " fmt "\n", x, y, ##__VA_ARGS__)
#define FONT_MED 0
#else
#include <bmp.h>
#include <menu.h>
#include <module.h>
#endif

#include "stream_engine.h"

typedef enum {
    TAB_SETTINGS = 0,
    TAB_PHOTO,
    TAB_CINE,
    TAB_ADDONS,
    TAB_HACKS,
    TAB_COUNT
} cinema_tab_t;

static const char *TAB_NAMES[] = {
    "SETTINGS", "PHOTO", "CINE", "ADD-ONS", "HACKS"
};

/* Tab accent colors (RGB565-ish conceptual; host stub ignores). */
static const uint32_t TAB_COLOR[] = {
    0xD4AF37, /* gold */
    0x3DDC84, /* green */
    0xE67E22, /* orange */
    0x3498DB, /* blue */
    0x9B59B6  /* purple */
};

typedef struct {
    const char *label;
    char value[48];
} cine_row_t;

static cinema_tab_t active_tab = TAB_CINE;
static int cine_sel = 2; /* Resolution highlighted like mockup */
static int battery_pct = 85;
static int bridge_present = 0;
static char clock_str[8] = "14:32";
static char storage_str[32] = "CF A: 64GB";

static cine_row_t cine_rows[] = {
    {"LUT", "RAW"},
    {"FRAME RATE", "25P"},
    {"Resolution", "UHD 4160x2560 [4K]"},
    {"CODEC", "CSP STREAM"},
    {"AUDIO", "STEREO"},
    {"ASPECT RATIO", "1.85:1"},
};

#define CINE_ROW_COUNT (int)(sizeof(cine_rows) / sizeof(cine_rows[0]))

void cinema_ui_set_bridge(int present)
{
    bridge_present = present;
    if (present)
        snprintf(storage_str, sizeof(storage_str), "CFast A: 128GB");
    else
        snprintf(storage_str, sizeof(storage_str), "CF A: 64GB");
}

void cinema_ui_apply_mode(const stream_mode_t *mode)
{
    if (!mode)
        return;
    snprintf(cine_rows[1].value, sizeof(cine_rows[1].value), "%dP",
             mode->fps_num / (mode->fps_den ? mode->fps_den : 1));
    snprintf(cine_rows[2].value, sizeof(cine_rows[2].value), "%dx%d [%s]",
             mode->width, mode->height, mode->mode_id);
    snprintf(cine_rows[3].value, sizeof(cine_rows[3].value), "CSP STREAM");
}

static void draw_status_bar(int y)
{
    bmp_printf(FONT_MED, 8, y, "%d%%", battery_pct);
    bmp_printf(FONT_MED, 300, y, "%s", clock_str);
    if (y > 400)
        bmp_printf(FONT_MED, 520, y, "%s", storage_str);
}

static void draw_tabs(void)
{
    int x = 20;
    for (int i = 0; i < TAB_COUNT; i++) {
        bmp_printf(FONT_MED, x, 36, "%s", TAB_NAMES[i]);
        if (i == (int)active_tab) {
            /* underline in tab accent — represented as marker on host */
            bmp_printf(FONT_MED, x, 52, "====");
            (void)TAB_COLOR[i];
        }
        x += 120;
    }
}

static void draw_cine_list(void)
{
    int y = 100;
    for (int i = 0; i < CINE_ROW_COUNT; i++) {
        if (i == cine_sel) {
            /* Copper hexagonal selection bar (mockup) */
            bmp_printf(FONT_MED, 40, y, "> %-14s  %s <", cine_rows[i].label, cine_rows[i].value);
        } else {
            bmp_printf(FONT_MED, 48, y, "%-14s  %s", cine_rows[i].label, cine_rows[i].value);
        }
        y += 40;
    }
}

void cinema_ui_draw(void)
{
    bmp_printf(FONT_MED, 340, 8, "MENU");
    draw_status_bar(8);
    draw_tabs();
    if (active_tab == TAB_CINE)
        draw_cine_list();
    draw_status_bar(450);
}

void cinema_ui_key(int key)
{
    /* key codes abstracted: 1=left 2=right 3=up 4=down 5=set */
    if (key == 1) {
        if (active_tab > 0)
            active_tab--;
    } else if (key == 2) {
        if (active_tab < TAB_COUNT - 1)
            active_tab++;
    } else if (key == 3) {
        if (cine_sel > 0)
            cine_sel--;
    } else if (key == 4) {
        if (cine_sel < CINE_ROW_COUNT - 1)
            cine_sel++;
    }
}

#ifdef CINEMA_UI_HOST_STUB
int main(void)
{
    cinema_ui_set_bridge(1);
    int n = 0;
    const stream_mode_t *modes = stream_mode_catalog(&n);
    cinema_ui_apply_mode(&modes[2]);
    cinema_ui_draw();
    return 0;
}
#endif
