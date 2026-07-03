/** \file
 * Cinema OS thermal guard — warn and trigger normal Canon shutdown when hot.
 */
#include "dryos.h"
#include "bmp.h"
#include "config.h"
#include "property.h"
#include "propvalues.h"
#include "beep.h"
#include "cinema_thermal.h"

static CONFIG_INT("cinema.thermal.enable", cinema_thermal_enable, 1);

#define CINE_THERMAL_WARN_C      92
#define CINE_THERMAL_SHUTDOWN_C  100

static int thermal_warn_active = 0;
static int thermal_shutdown_sent = 0;

int cinema_thermal_celsius(void)
{
#ifdef EFIC_CELSIUS
    return EFIC_CELSIUS;
#else
    return 0;
#endif
}

int cinema_thermal_warn_active(void) { return thermal_warn_active; }

void cinema_thermal_tick(void)
{
    if (!cinema_thermal_enable) return;

#ifdef EFIC_CELSIUS
    int t = EFIC_CELSIUS;
    if (t <= 0) return;

    if (t >= CINE_THERMAL_SHUTDOWN_C)
    {
        thermal_warn_active = 1;
        if (!thermal_shutdown_sent)
        {
            thermal_shutdown_sent = 1;
            NotifyBox(5000, "Thermal %dC — shutdown at 100C", t);
            beep();
            msleep(400);
            int req = 0;
            prop_request_change(PROP_TERMINATE_SHUT_REQ, &req, 4);
        }
        return;
    }

    if (t >= CINE_THERMAL_WARN_C)
    {
        thermal_warn_active = 1;
        /* keep recording — only warn, do not trigger Canon shutdown early */
    }
    else
    {
        thermal_warn_active = 0;
        thermal_shutdown_sent = 0;
    }
#else
    (void) thermal_warn_active;
#endif
}
