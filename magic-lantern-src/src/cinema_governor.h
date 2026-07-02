#ifndef _cinema_governor_h_
#define _cinema_governor_h_

/* Dynamic data-rate governor for sustained 4K MLV recording */

void cinema_governor_reset(void);
void cinema_governor_tick(void);
const char * cinema_governor_format_label(void);
int cinema_governor_fallback_active(void);

#endif
