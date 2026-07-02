/** \file
 * Resolve mlv_lite / crop_rec module symbols for Cine AI apply.
 */
#include "dryos.h"
#include <stdarg.h>
#include "module.h"
#include "cinema_module_bridge.h"

static int bridge_exec(const char * sym, int argc, ...)
{
    va_list args;
    va_start(args, argc);
    int ret = -1;

    switch (argc)
    {
        case 2:
            ret = module_exec(0, (char *) sym, 2,
                va_arg(args, uint32_t), va_arg(args, uint32_t));
            break;
        case 5:
            ret = module_exec(0, (char *) sym, 5,
                va_arg(args, uint32_t), va_arg(args, uint32_t),
                va_arg(args, uint32_t), va_arg(args, uint32_t),
                va_arg(args, uint32_t));
            break;
    }

    va_end(args);
    return ret;
}

int cinema_bridge_mlv_arm(
    int enable, int res_x, int aspect, int fmt, int preview_scale)
{
    int ret = bridge_exec("mlv_lite_cine_arm", 5,
        (unsigned int) enable, (unsigned int) res_x, (unsigned int) aspect,
        (unsigned int) fmt, (unsigned int) preview_scale);
    return ret > 0 ? 1 : 0;
}

int cinema_bridge_crop_apply(int crop_preset, int lv_res_pct)
{
    int ret = bridge_exec("crop_rec_cine_apply", 2,
        (unsigned int) crop_preset, (unsigned int) lv_res_pct);
    return ret > 0 ? 1 : 0;
}
