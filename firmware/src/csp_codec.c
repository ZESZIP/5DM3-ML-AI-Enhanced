#include "csp.h"
#include <string.h>

/* CRC32 (Ethernet polynomial) */
uint32_t csp_crc32(const void *data, uint32_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= p[i];
        for (int b = 0; b < 8; b++)
            crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)-(int)(crc & 1u));
    }
    return ~crc;
}

int csp_budget_bytes(int target_mibs, int fps_num, int fps_den)
{
    if (fps_num <= 0 || fps_den <= 0 || target_mibs <= 0)
        return 0;
    /* bytes/frame = target_mibs * 1024*1024 * den / num */
    long long num = (long long)target_mibs * 1024LL * 1024LL * fps_den;
    return (int)(num / fps_num);
}

int csp_pack12(const uint16_t *src, int n_samples, uint8_t *dst, int dst_max)
{
    int need = (n_samples * 3 + 1) / 2;
    if (n_samples & 1)
        need = ((n_samples + 1) / 2) * 3;
    else
        need = (n_samples / 2) * 3;
    if (need > dst_max)
        return -1;
    int o = 0;
    for (int i = 0; i + 1 < n_samples; i += 2) {
        uint16_t a = src[i] & 0x0FFF;
        uint16_t b = src[i + 1] & 0x0FFF;
        dst[o++] = (uint8_t)(a & 0xFF);
        dst[o++] = (uint8_t)(((a >> 8) & 0x0F) | ((b & 0x0F) << 4));
        dst[o++] = (uint8_t)((b >> 4) & 0xFF);
    }
    if (n_samples & 1) {
        uint16_t a = src[n_samples - 1] & 0x0FFF;
        dst[o++] = (uint8_t)(a & 0xFF);
        dst[o++] = (uint8_t)((a >> 8) & 0x0F);
        dst[o++] = 0;
    }
    return o;
}

int csp_unpack12(const uint8_t *src, int src_len, uint16_t *dst, int n_samples)
{
    int need = ((n_samples + 1) / 2) * 3;
    if (src_len < need)
        return -1;
    int s = 0;
    for (int i = 0; i + 1 < n_samples; i += 2) {
        uint8_t b0 = src[s++], b1 = src[s++], b2 = src[s++];
        dst[i] = (uint16_t)(b0 | ((b1 & 0x0F) << 8));
        dst[i + 1] = (uint16_t)(((b1 >> 4) & 0x0F) | (b2 << 4));
    }
    if (n_samples & 1) {
        uint8_t b0 = src[s++], b1 = src[s++];
        dst[n_samples - 1] = (uint16_t)(b0 | ((b1 & 0x0F) << 8));
    }
    return 0;
}

/* --- bit writer / reader for Exp-Golomb --- */
typedef struct {
    uint8_t *buf;
    int cap;
    int byte_pos;
    int bit_pos;
} bit_writer_t;

typedef struct {
    const uint8_t *buf;
    int len;
    int byte_pos;
    int bit_pos;
} bit_reader_t;

static void bw_init(bit_writer_t *w, uint8_t *buf, int cap)
{
    w->buf = buf;
    w->cap = cap;
    w->byte_pos = 0;
    w->bit_pos = 0;
    if (cap > 0)
        memset(buf, 0, (size_t)cap);
}

static int bw_write_bit(bit_writer_t *w, int bit)
{
    if (w->byte_pos >= w->cap)
        return -1;
    if (bit)
        w->buf[w->byte_pos] |= (uint8_t)(1u << (7 - w->bit_pos));
    w->bit_pos++;
    if (w->bit_pos == 8) {
        w->bit_pos = 0;
        w->byte_pos++;
    }
    return 0;
}

static int bw_write_bits(bit_writer_t *w, uint32_t v, int n)
{
    for (int i = n - 1; i >= 0; i--)
        if (bw_write_bit(w, (v >> i) & 1))
            return -1;
    return 0;
}

static int bw_bytes(bit_writer_t *w)
{
    return w->byte_pos + (w->bit_pos ? 1 : 0);
}

static int bw_eg(bit_writer_t *w, uint32_t v)
{
    /* unsigned Exp-Golomb: code = v+1 */
    uint32_t x = v + 1;
    int n = 0;
    uint32_t t = x;
    while (t) {
        n++;
        t >>= 1;
    }
    int zeros = n - 1;
    for (int i = 0; i < zeros; i++)
        if (bw_write_bit(w, 0))
            return -1;
    return bw_write_bits(w, x, n);
}

static void br_init(bit_reader_t *r, const uint8_t *buf, int len)
{
    r->buf = buf;
    r->len = len;
    r->byte_pos = 0;
    r->bit_pos = 0;
}

static int br_read_bit(bit_reader_t *r)
{
    if (r->byte_pos >= r->len)
        return -1;
    int bit = (r->buf[r->byte_pos] >> (7 - r->bit_pos)) & 1;
    r->bit_pos++;
    if (r->bit_pos == 8) {
        r->bit_pos = 0;
        r->byte_pos++;
    }
    return bit;
}

static int br_eg(bit_reader_t *r, uint32_t *out)
{
    int zeros = 0;
    for (;;) {
        int b = br_read_bit(r);
        if (b < 0)
            return -1;
        if (b == 1)
            break;
        zeros++;
        if (zeros > 31)
            return -1;
    }
    uint32_t x = 1;
    for (int i = 0; i < zeros; i++) {
        int b = br_read_bit(r);
        if (b < 0)
            return -1;
        x = (x << 1) | (uint32_t)b;
    }
    *out = x - 1;
    return 0;
}

static int encode_signed_eg(bit_writer_t *w, int32_t v)
{
    uint32_t u = (v <= 0) ? (uint32_t)(-2 * v) : (uint32_t)(2 * v - 1);
    return bw_eg(w, u);
}

static int decode_signed_eg(bit_reader_t *r, int32_t *v)
{
    uint32_t u;
    if (br_eg(r, &u))
        return -1;
    if (u & 1)
        *v = (int32_t)((u + 1) / 2);
    else
        *v = -(int32_t)(u / 2);
    return 0;
}

static int spatial_encode(
    const uint16_t *plane, int w, int h, int quant,
    bit_writer_t *bw)
{
    uint16_t left = 0, above = 0;
    for (int y = 0; y < h; y++) {
        left = 0;
        for (int x = 0; x < w; x++) {
            uint16_t sample = plane[y * w + x];
            uint16_t pred = (x == 0) ? (y == 0 ? 0 : above) : left;
            if (x == 0 && y > 0)
                pred = plane[(y - 1) * w + x];
            int32_t r = (int32_t)sample - (int32_t)pred;
            if (quant > 0) {
                int32_t sign = (r < 0) ? -1 : 1;
                r = sign * ((sign * r) >> quant);
            }
            if (encode_signed_eg(bw, r))
                return -1;
            left = sample;
            if (x == 0)
                above = sample;
        }
    }
    return 0;
}

static int spatial_decode(
    uint16_t *plane, int w, int h, int quant,
    bit_reader_t *br)
{
    for (int y = 0; y < h; y++) {
        uint16_t left = 0;
        for (int x = 0; x < w; x++) {
            uint16_t pred = (x == 0) ? (y == 0 ? 0 : plane[(y - 1) * w + x]) : left;
            int32_t r;
            if (decode_signed_eg(br, &r))
                return -1;
            if (quant > 0)
                r <<= quant;
            int32_t s = (int32_t)pred + r;
            if (s < 0)
                s = 0;
            if (s > 0x0FFF)
                s = 0x0FFF;
            plane[y * w + x] = (uint16_t)s;
            left = (uint16_t)s;
        }
    }
    return 0;
}

static void split_bayer(
    const uint16_t *raw, int w, int h,
    uint16_t *r, uint16_t *g1, uint16_t *g2, uint16_t *b)
{
    int pw = w / 2, ph = h / 2;
    for (int y = 0; y < ph; y++) {
        for (int x = 0; x < pw; x++) {
            int yy = y * 2, xx = x * 2;
            r[y * pw + x] = raw[yy * w + xx] & 0x0FFF;
            g1[y * pw + x] = raw[yy * w + xx + 1] & 0x0FFF;
            g2[y * pw + x] = raw[(yy + 1) * w + xx] & 0x0FFF;
            b[y * pw + x] = raw[(yy + 1) * w + xx + 1] & 0x0FFF;
        }
    }
}

static void merge_bayer(
    uint16_t *raw, int w, int h,
    const uint16_t *r, const uint16_t *g1, const uint16_t *g2, const uint16_t *b)
{
    int pw = w / 2, ph = h / 2;
    for (int y = 0; y < ph; y++) {
        for (int x = 0; x < pw; x++) {
            int yy = y * 2, xx = x * 2;
            raw[yy * w + xx] = r[y * pw + x];
            raw[yy * w + xx + 1] = g1[y * pw + x];
            raw[(yy + 1) * w + xx] = g2[y * pw + x];
            raw[(yy + 1) * w + xx + 1] = b[y * pw + x];
        }
    }
}

/* Small static scratch for embedded builds; host builds can pass larger frames
 * through the Python toolkit. Firmware modes stay within this envelope. */
#define CSP_MAX_PLANE ((2048 / 2) * (1280 / 2))
static uint16_t plane_r[CSP_MAX_PLANE];
static uint16_t plane_g1[CSP_MAX_PLANE];
static uint16_t plane_g2[CSP_MAX_PLANE];
static uint16_t plane_b[CSP_MAX_PLANE];
static uint16_t plane_tmp[CSP_MAX_PLANE * 4];

static int encode_spatial_frame(
    csp_encoder_t *enc, const uint16_t *raw, uint8_t *dst, int dst_max, int quant)
{
    int w = enc->width, h = enc->height;
    int pw = w / 2, ph = h / 2;
    if (pw * ph > CSP_MAX_PLANE)
        return -2;
    split_bayer(raw, w, h, plane_r, plane_g1, plane_g2, plane_b);
    bit_writer_t bw;
    bw_init(&bw, dst, dst_max);
    if (spatial_encode(plane_r, pw, ph, quant, &bw))
        return -1;
    if (spatial_encode(plane_g1, pw, ph, quant, &bw))
        return -1;
    if (spatial_encode(plane_g2, pw, ph, quant, &bw))
        return -1;
    if (spatial_encode(plane_b, pw, ph, quant, &bw))
        return -1;
    return bw_bytes(&bw);
}

static int decode_spatial_frame(
    csp_encoder_t *enc, const uint8_t *src, int src_len, uint16_t *dst, int quant)
{
    int w = enc->width, h = enc->height;
    int pw = w / 2, ph = h / 2;
    if (pw * ph > CSP_MAX_PLANE)
        return -2;
    bit_reader_t br;
    br_init(&br, src, src_len);
    if (spatial_decode(plane_r, pw, ph, quant, &br))
        return -1;
    if (spatial_decode(plane_g1, pw, ph, quant, &br))
        return -1;
    if (spatial_decode(plane_g2, pw, ph, quant, &br))
        return -1;
    if (spatial_decode(plane_b, pw, ph, quant, &br))
        return -1;
    merge_bayer(dst, w, h, plane_r, plane_g1, plane_g2, plane_b);
    return 0;
}

int csp_compress_frame(
    csp_encoder_t *enc,
    const uint16_t *raw,
    uint8_t *dst,
    int dst_max,
    int frame_index,
    int *out_type,
    int *out_quant)
{
    if (!enc || !raw || !dst)
        return -1;

    if (enc->compression == CSP_COMP_PACK12) {
        int n = enc->width * enc->height;
        int bytes = csp_pack12(raw, n, dst, dst_max);
        if (bytes < 0)
            return -1;
        if (out_type)
            *out_type = CSP_FRAME_I;
        if (out_quant)
            *out_quant = 0;
        if (enc->prev)
            memcpy(enc->prev, raw, (size_t)n * sizeof(uint16_t));
        return bytes;
    }

    int is_i = 1;
    if (enc->compression == CSP_COMP_TEMPORAL && enc->gop > 0)
        is_i = (frame_index % enc->gop) == 0 || !enc->prev;

    int q = enc->quant;
    for (;;) {
        int bytes;
        if (is_i || !enc->prev) {
            bytes = encode_spatial_frame(enc, raw, dst, dst_max, q);
            if (out_type)
                *out_type = CSP_FRAME_I;
        } else {
            int n = enc->width * enc->height;
            for (int i = 0; i < n; i++) {
                int32_t d = (int32_t)(raw[i] & 0x0FFF) - (int32_t)(enc->prev[i] & 0x0FFF);
                /* store biased residual into plane_tmp as unsigned 12-bit mid-gray */
                int32_t biased = d + 2048;
                if (biased < 0)
                    biased = 0;
                if (biased > 4095)
                    biased = 4095;
                plane_tmp[i] = (uint16_t)biased;
            }
            bytes = encode_spatial_frame(enc, plane_tmp, dst, dst_max, q);
            if (out_type)
                *out_type = CSP_FRAME_P;
        }
        if (bytes < 0)
            return bytes;
        if (enc->budget_bytes <= 0 || bytes <= enc->budget_bytes || q >= enc->q_max) {
            if (out_quant)
                *out_quant = q;
            if (enc->prev)
                memcpy(enc->prev, raw, (size_t)enc->width * enc->height * sizeof(uint16_t));
            return bytes;
        }
        q++;
    }
}

int csp_decompress_frame(
    csp_encoder_t *enc,
    const uint8_t *src,
    int src_len,
    uint16_t *dst,
    int frame_type,
    int quant)
{
    if (!enc || !src || !dst)
        return -1;
    int n = enc->width * enc->height;

    if (enc->compression == CSP_COMP_PACK12)
        return csp_unpack12(src, src_len, dst, n);

    if (frame_type == CSP_FRAME_I || !enc->prev) {
        if (decode_spatial_frame(enc, src, src_len, dst, quant))
            return -1;
    } else {
        if (decode_spatial_frame(enc, src, src_len, plane_tmp, quant))
            return -1;
        for (int i = 0; i < n; i++) {
            int32_t d = (int32_t)plane_tmp[i] - 2048;
            int32_t s = (int32_t)(enc->prev[i] & 0x0FFF) + d;
            if (s < 0)
                s = 0;
            if (s > 4095)
                s = 4095;
            dst[i] = (uint16_t)s;
        }
    }
    if (enc->prev)
        memcpy(enc->prev, dst, (size_t)n * sizeof(uint16_t));
    return 0;
}
