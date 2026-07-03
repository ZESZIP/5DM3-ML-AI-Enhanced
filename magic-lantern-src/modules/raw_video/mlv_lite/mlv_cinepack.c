/** \file
 * CINEPACK v3 + CSP stream — compiled into mlv_lite (no weak main-binary deps).
 */
#include "dryos.h"
#include "fio-ml.h"
#include "mlv_cinepack.h"

static int pack_active = 0;
static int pack_quality = 85;
static int target_centi_mbs = 0;
static int max_frame_bytes = 0;
static int last_frame_bytes = 0;
static int adaptive_thresh_extra = 0;
static int quant_lsb_shift = 0;
static int decim_shift = 0;

#define ROLLING_N 32
static int rolling_bytes[ROLLING_N];
static int rolling_idx = 0;
static int rolling_count = 0;
static int rolling_centimbs = 0;

static FILE * csp_file = 0;
static int csp_w = 0;
static int csp_h = 0;
static int csp_fps = 0;
static int csp_bpp = 14;
static int csp_frames = 0;
static int64_t csp_bytes = 0;
static int csp_active = 0;

int mlv_cinepack_active(void) { return pack_active; }
int mlv_cinepack_stream_mode(void) { return pack_active; }
int mlv_cinepack_quality(void) { return COERCE(pack_quality, 1, 100); }
int mlv_cinepack_max_frame_bytes(void) { return max_frame_bytes; }
int mlv_cinepack_last_frame_bytes(void) { return last_frame_bytes; }
int mlv_cinepack_rolling_centimbs(void) { return rolling_centimbs; }
int mlv_cinepack_frame_count(void) { return csp_frames; }

static void reset_governor(void)
{
    rolling_idx = 0;
    rolling_count = 0;
    rolling_centimbs = 0;
    adaptive_thresh_extra = 0;
    quant_lsb_shift = 0;
    decim_shift = 0;
    memset(rolling_bytes, 0, sizeof(rolling_bytes));
}

static int bytes_per_frame_budget(int centi_mbs, int fps_x1000)
{
    if (!centi_mbs || !fps_x1000) return 0;
    int64_t b = (int64_t) centi_mbs * 1024 * 1024 / 100;
    b = b * 1000 / fps_x1000;
    return (int) b;
}

void mlv_cinepack_arm(int on, int quality, int res_idx, int fps_idx, int beast)
{
    static const int fps_x1000[] = { 24000, 25000, 50000, 60000, 100000, 120000 };
    static const int res_w[] = { 1280, 1920, 2704, 3840, 5760 };

    pack_active = on ? 1 : 0;
    pack_quality = COERCE(quality, 1, 100);

    int fps = (fps_idx >= 0 && fps_idx < COUNT(fps_x1000)) ? fps_x1000[fps_idx] : 25000;
    int rw = (res_idx >= 0 && res_idx < COUNT(res_w)) ? res_w[res_idx] : 1920;

    if (beast == 2)
    {
        target_centi_mbs = MLV_TARGET_1080P120_CENTIMBS;
        fps = 120000;
    }
    else if (beast == 1)
    {
        target_centi_mbs = MLV_TARGET_4K25_CENTIMBS;
        rw = 3840;
        fps = 25000;
    }
    else if (res_idx >= 4)
    {
        target_centi_mbs = MLV_TARGET_6K25_CENTIMBS;
        fps = 25000;
    }
    else if (res_idx >= 3 || rw >= 3840)
        target_centi_mbs = MLV_TARGET_4K25_CENTIMBS;
    else if (fps >= 100000 || (rw <= 1920 && fps >= 50000))
        target_centi_mbs = MLV_TARGET_1080P120_CENTIMBS;
    else
        target_centi_mbs = MLV_TARGET_4K25_CENTIMBS;

    max_frame_bytes = bytes_per_frame_budget(target_centi_mbs, fps);
    reset_governor();

    printf("[CSP] arm on=%d q=%d target=%d cMB/s budget=%d B\n",
        pack_active, pack_quality, target_centi_mbs, max_frame_bytes);
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

    if (max_frame_bytes <= 0) return;
    int avg = sum / rolling_count;
    if (avg > max_frame_bytes * 92 / 100)
    {
        if (quant_lsb_shift < 4) quant_lsb_shift++;
        else if (decim_shift < 2) decim_shift++;
        adaptive_thresh_extra = MIN(24, adaptive_thresh_extra + 2);
    }
    else if (avg < max_frame_bytes * 70 / 100 && rolling_count >= 8)
    {
        if (adaptive_thresh_extra > 0) adaptive_thresh_extra--;
        if (quant_lsb_shift > 0 && avg < max_frame_bytes * 55 / 100) quant_lsb_shift--;
    }
}

static int16_t quantize_sample(int16_t v)
{
    if (!quant_lsb_shift) return v;
    return (v >> quant_lsb_shift) << quant_lsb_shift;
}

static int delta_threshold(int quality_pct)
{
    return MAX(1, 40 - quality_pct / 4) + adaptive_thresh_extra;
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
            int i = (y * step) * width + (x * step);
            if (i * 2 + 1 >= src_bytes) break;

            int16_t cur = quantize_sample(in[i]);
            int16_t d = cur - prev;
            prev = cur;
            if (ABS(d) <= thresh) d = 0;

            if (d == 0) { zero_run++; continue; }

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

void mlv_cinepack_set_target_centimbs(int centi_mbs)
{
    if (centi_mbs < 3000) centi_mbs = 3000;
    target_centi_mbs = centi_mbs;
    int fps = (target_centi_mbs == MLV_TARGET_1080P120_CENTIMBS) ? 120000 : 25000;
    max_frame_bytes = bytes_per_frame_budget(target_centi_mbs, fps);
    printf("[CSP] target adjusted %d cMB/s budget=%d\n", centi_mbs, max_frame_bytes);
}

int mlv_cinepack_compress(
    void * dst, int dst_max,
    const void * src, int src_bytes,
    int width, int height, int quality_pct)
{
    if (!pack_active || !dst || !src || dst_max < 64 || width < 16 || height < 2)
        return 0;

    uint8_t * out = dst;
    int q = COERCE(quality_pct, 1, 100);
    int budget = max_frame_bytes;
    int best_size = 0;
    int try_decim = decim_shift;

    for (int attempt = 0; attempt < 10; attempt++)
    {
        *(uint32_t *)(out + 0) = MLV_CINEPACK_MAGIC;
        *(uint16_t *)(out + 4) = MLV_CINEPACK_VERSION;
        *(uint16_t *)(out + 6) = (uint16_t) width;
        *(uint16_t *)(out + 8) = (uint16_t) height;
        out[10] = (uint8_t) q;
        out[11] = (uint8_t) MIN(target_centi_mbs / 100, 255);
        *(uint16_t *)(out + 12) = (uint16_t) adaptive_thresh_extra;
        out[14] = (uint8_t) quant_lsb_shift;
        out[15] = (uint8_t) try_decim;

        int sz = compress_pass(dst, dst_max, src, src_bytes, width, height, q, try_decim);
        if (sz < 0) return 0;

        best_size = sz;
        if (budget <= 0 || sz <= budget) break;

        adaptive_thresh_extra += 4;
        q = MAX(28, q - 6);
        if (attempt >= 2 && quant_lsb_shift < 4) quant_lsb_shift++;
        if (attempt >= 5 && try_decim < 2) try_decim++;
    }

    decim_shift = try_decim;
    last_frame_bytes = best_size;

    int fps = (target_centi_mbs == MLV_TARGET_1080P120_CENTIMBS) ? 120000 : 25000;
    governor_update(best_size, fps);
    return best_size;
}

#pragma pack(push,1)
typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t width;
    uint16_t height;
    uint32_t fps_x1000;
    uint8_t  bpp;
    uint8_t  codec;
    uint16_t target_centi_mbs;
    uint32_t frame_count;
    uint8_t  reserved[100];
} csp_file_hdr_t;

typedef struct {
    uint32_t magic;
    uint32_t payload_size;
    uint32_t frame_number;
    uint32_t reserved;
} csp_frame_hdr_t;
#pragma pack(pop)

int mlv_cinepack_begin_file(void * file, int width, int height, int fps_x1000, int bpp)
{
    if (!file || !pack_active) return 0;
    csp_file = (FILE *) file;
    csp_w = width;
    csp_h = height;
    csp_fps = fps_x1000;
    csp_bpp = bpp;
    csp_frames = 0;
    csp_bytes = sizeof(csp_file_hdr_t);
    csp_active = 1;
    reset_governor();

    csp_file_hdr_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = MLV_CSP_MAGIC;
    hdr.version = 2;
    hdr.width = (uint16_t) width;
    hdr.height = (uint16_t) height;
    hdr.fps_x1000 = (uint32_t) fps_x1000;
    hdr.bpp = (uint8_t) bpp;
    hdr.codec = MLV_CINEPACK_VERSION;
    hdr.target_centi_mbs = (uint16_t) target_centi_mbs;

    if (FIO_WriteFile(csp_file, &hdr, sizeof(hdr)) != sizeof(hdr))
    {
        printf("[CSP] header write failed\n");
        csp_active = 0;
        return 0;
    }
    printf("[CSP] stream start %dx%d fps=%d target=%d\n", width, height, fps_x1000, target_centi_mbs);
    return 1;
}

int mlv_cinepack_write_frame(const void * payload, int size, int frame_num)
{
    if (!csp_active || !csp_file || !payload || size <= 0) return 0;

    csp_frame_hdr_t fh;
    fh.magic = MLV_CSP_FRAME_MAGIC;
    fh.payload_size = (uint32_t) size;
    fh.frame_number = (uint32_t) frame_num;
    fh.reserved = 0;

    if (FIO_WriteFile(csp_file, &fh, sizeof(fh)) != sizeof(fh)) goto fail;
    if (FIO_WriteFile(csp_file, payload, size) != (uint32_t) size) goto fail;

    csp_frames++;
    csp_bytes += (int64_t) sizeof(fh) + size;
    if ((csp_frames % 8) == 0)
        FIO_SeekSkipFile(csp_file, 0, SEEK_CUR);
    return 1;

fail:
    printf("[CSP] frame %d write fail\n", frame_num);
    return 0;
}

void mlv_cinepack_end_file(void)
{
    if (!csp_file) return;
    if (csp_active)
    {
        csp_file_hdr_t hdr;
        memset(&hdr, 0, sizeof(hdr));
        hdr.magic = MLV_CSP_MAGIC;
        hdr.version = 2;
        hdr.width = (uint16_t) csp_w;
        hdr.height = (uint16_t) csp_h;
        hdr.fps_x1000 = (uint32_t) csp_fps;
        hdr.bpp = (uint8_t) csp_bpp;
        hdr.codec = MLV_CINEPACK_VERSION;
        hdr.target_centi_mbs = (uint16_t) target_centi_mbs;
        hdr.frame_count = (uint32_t) csp_frames;
        FIO_SeekSkipFile(csp_file, 0, SEEK_SET);
        FIO_WriteFile(csp_file, &hdr, sizeof(hdr));
        printf("[CSP] end frames=%d rolling=%d cMB/s\n", csp_frames, rolling_centimbs);
    }
    csp_active = 0;
    csp_file = 0;
}
