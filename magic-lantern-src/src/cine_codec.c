/** \file
 * CINEPACK v3 — row delta + RLE + decimation fallback + rolling bitrate governor.
 * Targets: 1080p120 < 92 MB/s, 4K25 < 72 MB/s, 6K25 < 95 MB/s.
 * No RAM ring — each frame compressed and flushed immediately.
 */
#include "dryos.h"
#include "config.h"
#include "cine_codec.h"

static CONFIG_INT("cine.codec.pack", cine_codec_pack, 0);
static CONFIG_INT("cine.codec.quality", cine_codec_quality_cfg, 85);
static CONFIG_INT("cine.codec.sensor_pct", cine_codec_sensor_pct_cfg, 100);
static CONFIG_INT("cine.codec.lv_pct", cine_codec_lv_pct_cfg, 50);
static CONFIG_INT("cine.codec.target_centi_mbs", cine_target_centi_mbs, 0);
static CONFIG_INT("cine.codec.max_frame_bytes", cine_max_frame_bytes_cfg, 0);

static int last_frame_bytes = 0;
static int last_ratio_pct = 100;
static int adaptive_thresh_extra = 0;
static int quant_lsb_shift = 0;
static int decim_shift = 0;

#define ROLLING_N 32
static int rolling_bytes[ROLLING_N];
static int rolling_idx = 0;
static int rolling_count = 0;
static int rolling_centimbs = 0;

int cine_codec_enabled(void) { return cine_codec_pack; }
int cine_codec_stream_mode(void) { return cine_codec_pack; }
int cine_codec_quality(void) { return COERCE(cine_codec_quality_cfg, 1, 100); }
int cine_codec_sensor_pct(void) { return COERCE(cine_codec_sensor_pct_cfg, 25, 100); }
int cine_codec_lv_pct(void) { return COERCE(cine_codec_lv_pct_cfg, 25, 100); }
int cine_codec_target_centimbs(void) { return cine_target_centi_mbs; }
int cine_codec_max_frame_bytes(void) { return cine_max_frame_bytes_cfg; }
int cine_codec_last_frame_bytes(void) { return last_frame_bytes; }
int cine_codec_last_ratio_pct(void) { return last_ratio_pct; }
int cine_codec_rolling_centimbs(void) { return rolling_centimbs; }

void cine_codec_set_mode(int use_cinepack, int quality_pct)
{
    cine_codec_pack = use_cinepack ? 1 : 0;
    cine_codec_quality_cfg = COERCE(quality_pct, 1, 100);
    adaptive_thresh_extra = 0;
    quant_lsb_shift = 0;
    decim_shift = 0;
}

void cine_codec_set_sensor_pct(int pct)
{
    cine_codec_sensor_pct_cfg = COERCE(pct, 25, 100);
}

void cine_codec_set_lv_pct(int pct)
{
    cine_codec_lv_pct_cfg = COERCE(pct, 25, 100);
}

void cine_codec_reset_governor(void)
{
    rolling_idx = 0;
    rolling_count = 0;
    rolling_centimbs = 0;
    adaptive_thresh_extra = 0;
    quant_lsb_shift = 0;
    decim_shift = 0;
    memset(rolling_bytes, 0, sizeof(rolling_bytes));
}

static int pct_to_lv_scale_index(int pct)
{
    if (pct >= 90) return 0;
    if (pct >= 65) return 1;
    if (pct >= 40) return 2;
    return 3;
}

int cine_codec_lv_scale_index(void)
{
    return pct_to_lv_scale_index(cine_codec_lv_pct());
}

int cine_codec_readout_pct_index(void)
{
    int pct = cine_codec_sensor_pct();
    if (pct >= 90) return 0;
    if (pct >= 65) return 1;
    if (pct >= 40) return 2;
    return 3;
}

static int bytes_per_frame_budget(int centi_mbs, int fps_x1000)
{
    if (!centi_mbs || !fps_x1000) return 0;
    int64_t b = (int64_t) centi_mbs * 1024 * 1024 / 100;
    b = b * 1000 / fps_x1000;
    return (int) b;
}

void cine_codec_set_record_profile(int res_idx, int fps_idx, int beast_mode)
{
    static const int fps_x1000[] = { 24000, 25000, 50000, 60000, 100000, 120000 };
    static const int res_w[] = { 1280, 1920, 2704, 3840, 5760 };

    int fps = (fps_idx >= 0 && fps_idx < COUNT(fps_x1000)) ? fps_x1000[fps_idx] : 25000;
    int rw = (res_idx >= 0 && res_idx < COUNT(res_w)) ? res_w[res_idx] : 1920;

    if (beast_mode == 2)
    {
        cine_target_centi_mbs = CINE_TARGET_1080P120_CENTIMBS;
        rw = 1920;
        fps = 120000;
    }
    else if (beast_mode == 1)
    {
        cine_target_centi_mbs = CINE_TARGET_4K25_CENTIMBS;
        rw = 3840;
        fps = 25000;
    }
    else if (res_idx >= 4)
    {
        cine_target_centi_mbs = CINE_TARGET_6K25_CENTIMBS;
        rw = 5760;
        fps = 25000;
    }
    else if (res_idx >= 3 || rw >= 3840)
    {
        cine_target_centi_mbs = CINE_TARGET_4K25_CENTIMBS;
    }
    else if (fps >= 100000 || (rw <= 1920 && fps >= 50000))
    {
        cine_target_centi_mbs = CINE_TARGET_1080P120_CENTIMBS;
    }
    else
    {
        cine_target_centi_mbs = CINE_TARGET_4K25_CENTIMBS;
    }

    int budget = bytes_per_frame_budget(cine_target_centi_mbs, fps);
    if (budget > 0)
        cine_max_frame_bytes_cfg = budget;

    cine_codec_reset_governor();
}

static void governor_update(int frame_bytes, int fps_x1000)
{
    rolling_bytes[rolling_idx] = frame_bytes;
    rolling_idx = (rolling_idx + 1) % ROLLING_N;
    if (rolling_count < ROLLING_N) rolling_count++;

    int sum = 0;
    for (int i = 0; i < rolling_count; i++)
        sum += rolling_bytes[i];

    if (fps_x1000 > 0 && rolling_count > 0)
    {
        int64_t bps = (int64_t) sum * fps_x1000 / rolling_count / 1000;
        rolling_centimbs = (int) (bps * 100 / 1024 / 1024);
    }

    int budget = cine_max_frame_bytes_cfg;
    if (budget <= 0) return;

    int avg = sum / rolling_count;
    if (avg > budget * 92 / 100)
    {
        if (quant_lsb_shift < 4)
            quant_lsb_shift++;
        else if (decim_shift < 2)
            decim_shift++;
        adaptive_thresh_extra = MIN(24, adaptive_thresh_extra + 2);
    }
    else if (avg < budget * 70 / 100 && rolling_count >= 8)
    {
        if (adaptive_thresh_extra > 0) adaptive_thresh_extra--;
        if (quant_lsb_shift > 0 && avg < budget * 55 / 100) quant_lsb_shift--;
    }
}

static int16_t quantize_sample(int16_t v)
{
    if (!quant_lsb_shift) return v;
    return (v >> quant_lsb_shift) << quant_lsb_shift;
}

static int delta_threshold(int quality_pct)
{
    int t = MAX(1, 40 - quality_pct / 4);
    return t + adaptive_thresh_extra;
}

static int emit_zero_run(uint8_t * out, int out_pos, int dst_max, int run)
{
    while (run > 0)
    {
        int chunk = MIN(run, 65535);
        if (out_pos + 4 > dst_max) return -1;
        if (chunk <= 255)
        {
            out[out_pos++] = 0x00;
            out[out_pos++] = (uint8_t) chunk;
        }
        else
        {
            out[out_pos++] = 0xFF;
            out[out_pos++] = 0x00;
            out[out_pos++] = (uint8_t)(chunk & 0xFF);
            out[out_pos++] = (uint8_t)((chunk >> 8) & 0xFF);
        }
        run -= chunk;
    }
    return out_pos;
}

static int emit_delta(uint8_t * out, int out_pos, int dst_max, int16_t d)
{
    if (d >= -127 && d <= 127)
    {
        if (out_pos + 1 > dst_max) return -1;
        out[out_pos++] = (uint8_t)(int8_t) d;
        return out_pos;
    }
    if (out_pos + 4 > dst_max) return -1;
    out[out_pos++] = 0xFF;
    out[out_pos++] = 0xFE;
    out[out_pos++] = (uint8_t)(d & 0xFF);
    out[out_pos++] = (uint8_t)((d >> 8) & 0xFF);
    return out_pos;
}

static int compress_pass(
    void * dst, int dst_max, const void * src, int src_bytes,
    int width, int height, int quality_pct, int decim)
{
    const uint16_t * in = src;
    uint8_t * out = dst;
    int out_pos = 16;
    int thresh = delta_threshold(quality_pct);
    int step = 1 << decim;
    int cw = width / step;
    int ch = height / step;
    if (cw < 16 || ch < 2) return -1;

    int zero_run = 0;

    for (int y = 0; y < ch; y++)
    {
        if (out_pos + 2 > dst_max) return -1;
        out[out_pos++] = 0xFD;
        out[out_pos++] = (uint8_t) y;

        int16_t prev = 0;
        for (int x = 0; x < cw; x++)
        {
            int sx = x * step;
            int sy = y * step;
            int i = sy * width + sx;
            if (i * 2 + 1 >= src_bytes) break;

            int16_t cur = quantize_sample(in[i]);
            int16_t d = cur - prev;
            prev = cur;

            if (ABS(d) <= thresh)
                d = 0;

            if (d == 0)
            {
                zero_run++;
                continue;
            }

            if (zero_run > 0)
            {
                out_pos = emit_zero_run(out, out_pos, dst_max, zero_run);
                if (out_pos < 0) return -1;
                zero_run = 0;
            }

            out_pos = emit_delta(out, out_pos, dst_max, d);
            if (out_pos < 0) return -1;
        }
    }

    if (zero_run > 0)
    {
        out_pos = emit_zero_run(out, out_pos, dst_max, zero_run);
        if (out_pos < 0) return -1;
    }

    return out_pos;
}

int cinepack_compress(
    void * dst, int dst_max,
    const void * src, int src_bytes,
    int width, int height, int quality_pct)
{
    if (!dst || !src || dst_max < 64 || width < 16 || height < 2)
        return 0;

    uint8_t * out = dst;
    int q = COERCE(quality_pct, 1, 100);
    int budget = cine_max_frame_bytes_cfg;
    int best_size = 0;
    int try_decim = decim_shift;

    for (int attempt = 0; attempt < 10; attempt++)
    {
        *(uint32_t *)(out + 0) = CINEPACK_MAGIC;
        *(uint16_t *)(out + 4) = CINEPACK_VERSION;
        *(uint16_t *)(out + 6) = (uint16_t) width;
        *(uint16_t *)(out + 8) = (uint16_t) height;
        out[10] = (uint8_t) q;
        out[11] = (uint8_t) MIN(cine_target_centi_mbs / 100, 255);
        *(uint16_t *)(out + 12) = (uint16_t) adaptive_thresh_extra;
        out[14] = (uint8_t) quant_lsb_shift;
        out[15] = (uint8_t) try_decim;

        int sz = compress_pass(dst, dst_max, src, src_bytes, width, height, q, try_decim);
        if (sz < 0) return 0;

        best_size = sz;
        if (budget <= 0 || sz <= budget)
            break;

        adaptive_thresh_extra += 4;
        q = MAX(28, q - 6);
        if (attempt >= 2 && quant_lsb_shift < 4)
            quant_lsb_shift++;
        if (attempt >= 5 && try_decim < 2)
            try_decim++;
    }

    decim_shift = try_decim;
    last_frame_bytes = best_size;
    if (src_bytes > 0)
        last_ratio_pct = best_size * 100 / src_bytes;

    static const int fps_x1000[] = { 24000, 25000, 50000, 60000, 100000, 120000 };
    int fps = 25000;
    if (cine_target_centi_mbs == CINE_TARGET_1080P120_CENTIMBS)
        fps = 120000;
    governor_update(best_size, fps);

    return best_size;
}
