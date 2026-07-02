/** \file
 * Cine AI Enhanced — built-in text help (no BMH files required).
 */
#include "dryos.h"
#include "bmp.h"
#include "font.h"
#include "menuhelp.h"

static void cine_help_draw(const char * title, const char * body)
{
    bmp_fill(COLOR_BLACK, 0, 0, 720, 480);
    bmp_fill(COLOR_ORANGE, 0, 0, 720, 6);
    bmp_printf(FONT(FONT_CANON, COLOR_WHITE, COLOR_BLACK), 24, 24, "%s", title);
    bmp_printf(FONT(FONT_MED, COLOR_GRAY(55), COLOR_BLACK), 24, 70, "%s", body);
    bmp_printf(FONT(FONT_SMALL, COLOR_ORANGE, COLOR_BLACK), 24, 440,
        "CINE AI ENHANCED  |  Wheel: pages  |  MENU: back");
}

void cinema_help_show_label(const char * label)
{
    if (!label || !label[0])
    {
        cine_help_draw("CINE AI ENHANCED",
            "Five-page cinema OS for 5D Mark III.\n"
            "CINE tab: resolution, FPS, format, gamma,\n"
            "shutter, aperture, ISO, WB and audio.\n"
            "Press SET on any row for thick in-window panels.");
        return;
    }

    if (streq(label, "Cinema Record") || streq(label, "RAW video"))
    {
        cine_help_draw("RECORDING",
            "Use CINE page rows or Cinema Record for MLV.\n"
            "BEAST 4K25 and 1080p120 profiles auto-apply\n"
            "crop, bit depth and FPS for your CF card speed.");
        return;
    }

    if (streq(label, "FPS override"))
    {
        cine_help_draw("FRAME RATE",
            "Override Canon movie FPS for slow/fast motion.\n"
            "Set from CINE > FRAME RATE panel or Movie menu.\n"
            "High FPS may need 720p in Canon menu.");
        return;
    }

    cine_help_draw(label,
        "Adjust this setting from the CINE page panel\n"
        "or the matching submenu. Help files are built\n"
        "into Cine AI Enhanced — no card install needed.");
}
