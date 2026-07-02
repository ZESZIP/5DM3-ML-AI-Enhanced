/** \file
 * Cine AI Enhanced — MLV / module debug ring-buffer → ML/LOGS/CINE_DEBUG.LOG
 */
#include "dryos.h"
#include "bmp.h"
#include "font.h"
#include "config.h"
#include "fio-ml.h"
#include "module.h"
#include "propvalues.h"
#include "version.h"
#include "menu.h"
#include "cinema_debug.h"
#include "cinema_record_apply.h"

static CONFIG_INT("cinema.debug.overlay", cine_debug_overlay, 1);

#define CINE_DEBUG_LINES  32
#define CINE_DEBUG_LINE_LEN 96

static char debug_lines[CINE_DEBUG_LINES][CINE_DEBUG_LINE_LEN];
static int debug_line_head = 0;
static int debug_line_count = 0;

void cine_debug_init(void)
{
    FIO_CreateDirectory("ML/LOGS");
    debug_line_head = 0;
    debug_line_count = 0;
    cine_debug_log("=== CINE DEBUG %s ===", build_version);
}

static void debug_push_line(const char * line)
{
    snprintf(debug_lines[debug_line_head], CINE_DEBUG_LINE_LEN, "%s", line);
    debug_line_head = (debug_line_head + 1) % CINE_DEBUG_LINES;
    if (debug_line_count < CINE_DEBUG_LINES)
        debug_line_count++;
}

void cine_debug_log(const char * fmt, ...)
{
    char line[CINE_DEBUG_LINE_LEN];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);

    char stamped[CINE_DEBUG_LINE_LEN];
    snprintf(stamped, sizeof(stamped), "%dms %s", get_ms_clock(), line);
    debug_push_line(stamped);
    printf("[CINE] %s\n", stamped);
}

void cine_debug_flush(void)
{
    FIO_CreateDirectory("ML/LOGS");

    FILE * f = FIO_CreateFileOrAppend(CINE_DEBUG_LOG_PATH);
    if (!f)
    {
        printf("[CINE] cannot open %s\n", CINE_DEBUG_LOG_PATH);
        return;
    }

    int start = (debug_line_count < CINE_DEBUG_LINES) ? 0 : debug_line_head;
    for (int i = 0; i < debug_line_count; i++)
    {
        int idx = (start + i) % CINE_DEBUG_LINES;
        char out[CINE_DEBUG_LINE_LEN + 2];
        snprintf(out, sizeof(out), "%s\n", debug_lines[idx]);
        FIO_WriteFile(f, out, strlen(out));
    }

    FIO_CloseFile(f);
}

void cine_debug_log_modules(void)
{
    cine_debug_log("--- module status ---");
    module_cine_debug_log(cine_debug_log);

    int mod = -1;
    int loaded = 0;
    while ((mod = module_get_next_loaded(mod)) >= 0)
        loaded++;

    cine_debug_log("loaded module count: %d", loaded);

    static const char * critical[] = { "mlv_lite", "crop_rec", NULL };
    for (int c = 0; critical[c]; c++)
    {
        int ok = 0;
        int m = -1;
        while ((m = module_get_next_loaded(m)) >= 0)
        {
            if (strcmp(module_get_name(m), critical[c]) == 0)
            {
                ok = 1;
                break;
            }
        }
        cine_debug_log("critical %s: %s", critical[c], ok ? "LOADED" : "MISSING");
    }
}

void cine_debug_log_mlv_state(void)
{
    cine_debug_log("--- MLV state ---");
    cine_debug_log("lv=%d movie=%d rec=%d",
        lv ? 1 : 0, is_movie_mode() ? 1 : 0, RECORDING ? 1 : 0);
    cine_debug_log("raw.video.enabled=%d", get_config_var("raw.video.enabled"));
    cine_debug_log("raw.res_x=%d fmt=%d lv_scale=%d",
        get_config_var("raw.res_x"),
        get_config_var("raw.output_format"),
        get_config_var("raw.preview.lv_scale"));
    cine_debug_log("crop.preset=%d crop.lv=%d",
        get_config_var("crop.preset"),
        get_config_var("crop.lv_res_pct"));
    cine_debug_log("cine.codec.pack=%d bpp=%s peaking=%d",
        get_config_var("cine.codec.pack"),
        cinema_record_bpp_label(),
        cinema_record_peaking_on());
}

int cine_debug_overlay_enabled(void) { return cine_debug_overlay; }
int* cine_debug_overlay_var(void) { return &cine_debug_overlay; }

void cine_debug_draw_overlay(void)
{
    if (!cine_debug_overlay || !debug_line_count) return;

    int lines = MIN(4, debug_line_count);
    int y = 4;
    bmp_fill(COLOR_GRAY(8), 4, y, 712, lines * 14 + 8);
    bmp_draw_rect_chamfer(COLOR_GRAY(40), 4, y, 712, lines * 14 + 8, 3, 0);

    int start = debug_line_head - lines;
    if (start < 0) start += CINE_DEBUG_LINES;

    for (int i = 0; i < lines; i++)
    {
        int idx = (start + i) % CINE_DEBUG_LINES;
        bmp_printf(FONT(FONT_SMALL, COLOR_GREEN1, COLOR_GRAY(8)),
            8, y + 4 + i * 14, "%s", debug_lines[idx]);
    }
}

static MENU_UPDATE_FUNC(cine_debug_menu_update)
{
    MENU_SET_VALUE(cine_debug_overlay ? "ON" : "OFF");
    MENU_SET_HELP("On-screen MLV debug lines while menu is open.");
}

static MENU_SELECT_FUNC(cine_debug_menu_select)
{
    (void) priv;
    (void) delta;
    cine_debug_log_modules();
    cine_debug_log_mlv_state();
    cine_debug_flush();
    NotifyBox(5000, "Debug saved:\nML/LOGS/CINE_DEBUG.LOG\nUpload this file.");
}

static struct menu_entry cine_debug_menu[] = {
    {
        .name = "CINE debug overlay",
        .priv = cine_debug_overlay_var,
        .max = 1,
        .choices = CHOICES("OFF", "ON"),
        .update = cine_debug_menu_update,
        .help = "Show last MLV debug lines on screen.",
    },
    {
        .name = "Export CINE debug log",
        .select = cine_debug_menu_select,
        .help = "Write ML/LOGS/CINE_DEBUG.LOG — upload to diagnose MLV failures.",
    },
    MENU_EOL,
};

static void cine_debug_menu_init_task(void * unused)
{
    (void) unused;
    menu_add("Debug", cine_debug_menu, COUNT(cine_debug_menu) - 1);
}

INIT_FUNC(__FILE__, cine_debug_menu_init_task);
