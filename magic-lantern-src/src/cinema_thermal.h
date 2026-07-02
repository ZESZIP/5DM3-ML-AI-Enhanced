#ifndef _cinema_thermal_h_
#define _cinema_thermal_h_

/* Thermal guard — restore Canon shutdown when body overheats. */

void cinema_thermal_tick(void);
int cinema_thermal_celsius(void);
int cinema_thermal_warn_active(void);

#endif
