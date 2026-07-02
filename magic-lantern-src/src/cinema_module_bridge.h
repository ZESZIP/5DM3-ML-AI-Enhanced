#ifndef _cinema_module_bridge_h_
#define _cinema_module_bridge_h_

#include "module.h"

typedef unsigned int (*mlv_lite_cine_arm_fn)(
    unsigned int enable, unsigned int res_x_idx, unsigned int aspect,
    unsigned int fmt, unsigned int preview_scale);

typedef unsigned int (*crop_rec_cine_apply_fn)(
    unsigned int preset_index, unsigned int lv_res_pct_idx);

int cinema_bridge_mlv_arm(
    int enable, int res_x, int aspect, int fmt, int preview_scale);

int cinema_bridge_crop_apply(int crop_preset, int lv_res_pct);

#endif
