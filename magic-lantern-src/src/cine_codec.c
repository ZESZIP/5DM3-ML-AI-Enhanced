/** \file
 * CINEPACK v1 — horizontal delta + RLE for 5D3 raw frames.
 * Designed for Digic 5 real-time encode; decode on PC to standard MLV.
 */
#include "dryos.h"
#include "config.h"
#include "cine_codec.h"

static CONFIG_INT("cine.codec.pack", cine_codec_pack, 0);
static CONFIG_INT("cine.codec.quality", cine_codec_quality_cfg, 85);
static CONFIG_INT("cine.codec.sensor_pct", cine_codec_sensor_pct_cfg, 100);
static CONFIG_INT("cine.codec.lv_pct", cine_codec_lv_pct_cfg, 50);

int cine_codec_enabled(void) { return cine_codec_pack; }
int cine_codec_quality(void) { return COERCE(cine_codec_quality_cfg, 1, 100); }
int cine_codec_sensor_pct(void) { return COERCE(cine_codec_sensor_pct_cfg, 25, 100); }
int cine_codec_lv_pct(void) { return COERCE(cine_codec_lv_pct_cfg, 25, 100); }

void cine_codec_set_mode(int use_cinepack, int quality_pct)
{
    cine_codec_pack = use_cinepack ? 1 : 0;
    cine_codec_quality_cfg = COERCE(quality_pct, 1, 100);
}

void cine_codec_set_sensor_pct(int pct)
{
    cine_codec_sensor_pct_cfg = COERCE(pct, 25, 100);
}

void cine_codec_set_lv_pct(int pct)
{
    cine_codec_lv_pct_cfg = COERCE(pct, 25, 100);
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

/* threshold scales with quality: higher quality = keep smaller deltas */
static int delta_threshold(int quality_pct)
{
    return MAX(2, 64 - quality_pct / 2);
}

int cinepack_compress(
    void * dst, int dst_max,
    const void * src, int src_bytes,
    int width, int height, int quality_pct)
{
    if (!dst || !src || dst_max < 64 || width < 16 || height < 2)
        return 0;

    const uint16_t * in = src;
    uint8_t * out = dst;
    int out_pos = 0;

    /* header: magic, version, w, h, quality, flags */
    if (dst_max < 16) return 0;
    *(uint32_t *)(out + 0) = CINEPACK_MAGIC;
    *(uint16_t *)(out + 4) = 1; /* version */
    *(uint16_t *)(out + 6) = (uint16_t) width;
    *(uint16_t *)(out + 8) = (uint16_t) height;
    out[10] = (uint8_t) quality_pct;
    out[11] = 0;
    out_pos = 16;

    int thresh = delta_threshold(quality_pct);
    int pixels = width * height;
    if (pixels * 2 > src_bytes) pixels = src_bytes / 2;

    int16_t prev = 0;
    for (int i = 0; i < pixels; i++)
    {
        int16_t cur = in[i];
        int16_t d = cur - prev;
        prev = cur;

        if (ABS(d) <= thresh)
            d = 0;

        /* RLE: 0xFF escape, then int16 delta */
        if (d == 0)
        {
            int run = 1;
            while (i + run < pixels && in[i + run] - in[i + run - 1] == 0)
            {
                if (ABS(in[i + run] - in[i + run - 1]) > thresh) break;
                int16_t p = in[i + run - 1];
                int16_t nd = in[i + run] - p;
                if (nd != 0) break;
                run++;
            }
            if (run >= 4)
            {
                if (out_pos + 3 > dst_max) return 0;
                out[out_pos++] = 0xFF;
                out[out_pos++] = (uint8_t) MIN(run, 255);
                out[out_pos++] = 0;
                i += run - 1;
                prev = in[i];
                continue;
            }
            d = 0;
        }

        if (out_pos + 3 > dst_max) return 0;
        if (d >= -127 && d <= 127)
        {
            out[out_pos++] = (uint8_t)(int8_t) d;
        }
        else
        {
            out[out_pos++] = 0xFE;
            out[out_pos++] = (uint8_t)(d & 0xFF);
            out[out_pos++] = (uint8_t)((d >> 8) & 0xFF);
        }
    }

    return out_pos;
}
