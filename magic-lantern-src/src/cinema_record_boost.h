#ifndef _cinema_record_boost_h_
#define _cinema_record_boost_h_

/* Aggressive 5D3 recording mode — starve background work, push card/CPU to limit. */

void cinema_record_boost_enter(void);
void cinema_record_boost_exit(void);
void cinema_record_boost_tick(void);
int cinema_record_boost_active(void);

#endif
