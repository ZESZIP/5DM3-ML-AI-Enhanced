#include "stream_engine.h"
#include "csp.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

static const stream_mode_t CATALOG[] = {
    {"FF1080P25",     1920, 1080, 25,  1, 12, CSP_COMP_SPATIAL,  45, 0},
    {"FF1080P50",     1920, 1080, 50,  1, 12, CSP_COMP_SPATIAL,  70, 0},
    {"CROP1080P100",  1920, 1080, 100, 1, 12, CSP_COMP_TEMPORAL, 95, 0},
    {"CROP4K25",      3840, 2160, 25,  1, 12, CSP_COMP_TEMPORAL, 90, 0},
    {"CROP4K50_LITE", 3840, 1600, 50,  1, 12, CSP_COMP_TEMPORAL, 95, 0},
    {"CROP4K50_FULL", 3840, 2160, 50,  1, 12, CSP_COMP_TEMPORAL,150, 1},
};

static const stream_mode_t *armed;
static stream_metrics_t metrics;
static csp_encoder_t enc;
static uint16_t *prev_frame;
static uint8_t *comp_buf;
static int comp_cap;
static FILE *out;
static csp_header_t hdr;
static int frame_index;

const stream_mode_t *stream_mode_catalog(int *count)
{
    if (count)
        *count = (int)(sizeof(CATALOG) / sizeof(CATALOG[0]));
    return CATALOG;
}

const stream_mode_t *stream_select_mode(uint32_t measured_mibs, int bridge_present)
{
    uint32_t ceiling = measured_mibs * 90 / 100;
    if (!bridge_present && ceiling > 100)
        ceiling = 100;
    if (bridge_present && ceiling > 160)
        ceiling = 160;

    /* Prefer higher modes first */
    for (int i = (int)(sizeof(CATALOG) / sizeof(CATALOG[0])) - 1; i >= 0; i--) {
        if (CATALOG[i].needs_bridge && !bridge_present)
            continue;
        if (CATALOG[i].target_mibs <= ceiling)
            return &CATALOG[i];
    }
    return &CATALOG[0];
}

int stream_engine_arm(const stream_mode_t *mode)
{
    armed = mode;
    memset(&metrics, 0, sizeof(metrics));
    return mode ? 0 : -1;
}

static void fill_header(void)
{
    memset(&hdr, 0, sizeof(hdr));
    memcpy(hdr.magic, "CSP1", 4);
    hdr.version = CSP_VERSION;
    hdr.header_size = CSP_HEADER_SIZE;
    hdr.width = armed->width;
    hdr.height = armed->height;
    hdr.fps_num = armed->fps_num;
    hdr.fps_den = armed->fps_den;
    hdr.bit_depth = armed->bit_depth;
    hdr.bayer = 0;
    hdr.compression = armed->compression;
    hdr.log_tag = 1; /* CSP-Log offline */
    hdr.black_level = 2047;
    hdr.white_level = 15000;
    hdr.iso = 400;
    hdr.shutter_us = 1000000 / (armed->fps_num > 0 ? armed->fps_num : 25);
    strncpy(hdr.mode_id, armed->mode_id, sizeof(hdr.mode_id) - 1);
    hdr.created_unix = (uint64_t)time(NULL);
    hdr.flags = CSP_FLAG_LOSSLESS_I | CSP_FLAG_HAS_INDEX;
}

int stream_engine_start(const char *path)
{
    if (!armed || !path)
        return -1;
    free(prev_frame);
    free(comp_buf);
    prev_frame = (uint16_t *)calloc((size_t)armed->width * armed->height, sizeof(uint16_t));
    comp_cap = armed->width * armed->height * 2;
    comp_buf = (uint8_t *)malloc((size_t)comp_cap);
    if (!prev_frame || !comp_buf)
        return -1;

    memset(&enc, 0, sizeof(enc));
    enc.width = armed->width;
    enc.height = armed->height;
    enc.bit_depth = armed->bit_depth;
    enc.compression = armed->compression;
    enc.gop = 12;
    enc.quant = 0;
    enc.q_max = 6;
    enc.budget_bytes = csp_budget_bytes(armed->target_mibs, armed->fps_num, armed->fps_den);
    enc.black_level = 2047;
    enc.prev = prev_frame;

    out = fopen(path, "wb");
    if (!out)
        return -1;
    fill_header();
    if (fwrite(&hdr, 1, sizeof(hdr), out) != sizeof(hdr))
        return -1;
    frame_index = 0;
    metrics.recording = 1;
    return 0;
}

stream_status_t stream_engine_tick(const uint16_t *raw_frame)
{
    if (!metrics.recording || !out || !raw_frame)
        return STREAM_IO;

    int type = 0, quant = 0;
    int bytes = csp_compress_frame(&enc, raw_frame, comp_buf, comp_cap, frame_index, &type, &quant);
    if (bytes < 0)
        return STREAM_BUDGET;
    if (enc.budget_bytes > 0 && bytes > enc.budget_bytes && quant >= enc.q_max) {
        metrics.underruns++;
        return STREAM_BUDGET;
    }

    csp_frame_hdr_t fh;
    memset(&fh, 0, sizeof(fh));
    memcpy(fh.magic, "CSFR", 4);
    fh.index = (uint32_t)frame_index;
    fh.timestamp_us = (uint64_t)frame_index * 1000000ULL * armed->fps_den / armed->fps_num;
    fh.type = (uint8_t)type;
    fh.quant = (uint8_t)quant;
    fh.payload_size = (uint32_t)bytes;
    fh.crc32 = csp_crc32(comp_buf, (uint32_t)bytes);

    if (fwrite(&fh, 1, sizeof(fh), out) != sizeof(fh))
        return STREAM_IO;
    if (fwrite(comp_buf, 1, (size_t)bytes, out) != (size_t)bytes)
        return STREAM_IO;

    metrics.last_frame_bytes = (uint32_t)bytes;
    metrics.frames_ok++;
    /* rolling MB/s * 100 */
    uint32_t fps = armed->fps_num / (armed->fps_den ? armed->fps_den : 1);
    if (fps == 0)
        fps = 1;
    metrics.write_centi_mbs = (uint32_t)((uint64_t)bytes * fps * 100 / (1024 * 1024));
    frame_index++;
    return STREAM_OK;
}

int stream_engine_stop(void)
{
    if (out) {
        fclose(out);
        out = NULL;
    }
    metrics.recording = 0;
    free(prev_frame);
    prev_frame = NULL;
    free(comp_buf);
    comp_buf = NULL;
    return 0;
}

const stream_metrics_t *stream_engine_metrics(void)
{
    return &metrics;
}
