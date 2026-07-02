#ifndef _cinema_record_apply_h_
#define _cinema_record_apply_h_

/* Cine AI — apply recording profile and arm MLV or MOV */

typedef enum {
    CINE_REC_MOV = 0,
    CINE_REC_MLV = 1,
} cine_rec_container_t;

/* Panel indices (stored in config) */
void cinema_record_set_beast(int beast);
void cinema_record_set_resolution(int res_idx);
void cinema_record_set_fps(int fps_idx);
void cinema_record_set_format_idx(int fmt_idx);
void cinema_record_set_sensor_pct(int pct);
void cinema_record_set_lv_pct(int pct);
int cinema_record_lv_pct(void);

int cinema_record_format_is_cinepack(void);
int cinema_record_apply_full(void);
int cinema_record_mlv_armed(void);
const char * cinema_record_container_label(void);
const char * cinema_record_format_label(void);
const char * cinema_record_resolution_label(void);
const char * cinema_record_fps_label(void);

#endif
