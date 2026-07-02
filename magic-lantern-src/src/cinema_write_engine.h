#ifndef _cinema_write_engine_h_
#define _cinema_write_engine_h_

/* Cinema OS write engine — card benchmark, profile auto-apply, hack arming */

void cinema_write_arm_hacks(void);
void cinema_write_benchmark_quiet(void);
void cinema_write_apply_best_profile(void);
void cinema_write_engine_tick(void);

int cinema_write_speed_centi_mbs(void);
const char * cinema_write_speed_label(void);
const char * cinema_write_profile_label(void);
int cinema_write_engine_ready(void);

#endif
