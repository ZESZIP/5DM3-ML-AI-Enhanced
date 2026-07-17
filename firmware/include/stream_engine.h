#ifndef CINEMA_OS_STREAM_ENGINE_H
#define CINEMA_OS_STREAM_ENGINE_H

#include <stdint.h>

typedef enum {
    STREAM_OK = 0,
    STREAM_UNDERRUN = 1,
    STREAM_THERMAL = 2,
    STREAM_IO = 3,
    STREAM_BUDGET = 4
} stream_status_t;

typedef struct stream_metrics {
    uint32_t write_centi_mbs; /* 100 = 1.00 MB/s */
    uint32_t last_frame_bytes;
    uint32_t frames_ok;
    uint32_t underruns;
    uint8_t  thermal_flag;
    uint8_t  recording;
} stream_metrics_t;

typedef struct stream_mode {
    const char *mode_id;
    uint16_t width;
    uint16_t height;
    uint16_t fps_num;
    uint16_t fps_den;
    uint8_t  bit_depth;
    uint8_t  compression; /* CSP_COMP_* */
    uint16_t target_mibs;
    uint8_t  needs_bridge;
} stream_mode_t;

const stream_mode_t *stream_mode_catalog(int *count);
const stream_mode_t *stream_select_mode(uint32_t measured_mibs, int bridge_present);

int stream_engine_arm(const stream_mode_t *mode);
int stream_engine_start(const char *path);
int stream_engine_stop(void);
stream_status_t stream_engine_tick(const uint16_t *raw_frame);
const stream_metrics_t *stream_engine_metrics(void);

#endif
