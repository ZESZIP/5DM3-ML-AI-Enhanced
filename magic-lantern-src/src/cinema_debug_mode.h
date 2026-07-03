#ifndef _cinema_debug_mode_h_
#define _cinema_debug_mode_h_

/* Global debug gate — when OFF, internal debug hooks stay disabled. */

int cinema_debug_mode_enabled(void);
int * cinema_debug_mode_var(void);

#endif
