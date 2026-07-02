#ifndef _cinema_debug_h_
#define _cinema_debug_h_

#define CINE_DEBUG_LOG_PATH "ML/LOGS/CINE_DEBUG.LOG"

void cine_debug_init(void);
void cine_debug_log(const char * fmt, ...) __attribute__((format(printf, 1, 2)));
void cine_debug_flush(void);
void cine_debug_log_modules(void);
void cine_debug_log_mlv_state(void);
void cine_debug_draw_overlay(void);
int cine_debug_overlay_enabled(void);
int* cine_debug_overlay_var(void);

#endif
