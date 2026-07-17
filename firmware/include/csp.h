/* Cinema Stream Pack — portable on-camera codec API (DIGIC / host). */
#ifndef CINEMA_OS_CSP_H
#define CINEMA_OS_CSP_H

#include <stdint.h>

#define CSP_MAGIC_FILE   0x31505343u /* 'CSP1' LE */
#define CSP_MAGIC_FRAME  0x52465343u /* 'CSFR' LE */
#define CSP_MAGIC_AUDIO  0x55415343u /* 'CSAU' LE */
#define CSP_MAGIC_INDEX  0x58495343u /* 'CSIX' LE */

#define CSP_VERSION      1
#define CSP_HEADER_SIZE  256

#define CSP_COMP_PACK12    0
#define CSP_COMP_SPATIAL   1
#define CSP_COMP_TEMPORAL  2

#define CSP_FRAME_I  0
#define CSP_FRAME_P  1

#define CSP_FLAG_DUAL_STRIPE  (1u << 0)
#define CSP_FLAG_SSD_BRIDGE   (1u << 1)
#define CSP_FLAG_LOSSLESS_I   (1u << 2)
#define CSP_FLAG_HAS_INDEX    (1u << 3)

typedef struct csp_header {
    char     magic[4];
    uint16_t version;
    uint16_t header_size;
    uint16_t width;
    uint16_t height;
    uint16_t fps_num;
    uint16_t fps_den;
    uint8_t  bit_depth;
    uint8_t  bayer;
    uint8_t  compression;
    uint8_t  log_tag;
    uint32_t flags;
    uint16_t black_level;
    uint16_t white_level;
    uint32_t iso;
    uint32_t shutter_us;
    int32_t  color_matrix[9];
    char     mode_id[32];
    uint32_t reserved0;
    uint64_t created_unix;
    uint32_t audio_rate;
    uint16_t audio_ch;
    uint16_t audio_bits;
    uint8_t  pad[132];
} csp_header_t;

typedef struct csp_frame_hdr {
    char     magic[4];
    uint32_t index;
    uint64_t timestamp_us;
    uint8_t  type;
    uint8_t  quant;
    uint16_t reserved;
    uint32_t payload_size;
    uint32_t crc32;
} csp_frame_hdr_t;

typedef struct csp_encoder {
    int width;
    int height;
    int bit_depth;
    int compression;
    int gop;
    int quant;
    int q_max;
    int budget_bytes;
    uint16_t black_level;
    uint16_t *prev; /* previous decoded frame for temporal */
} csp_encoder_t;

uint32_t csp_crc32(const void *data, uint32_t len);

/* Pack/unpack tightly packed 12-bit samples (2 pixels -> 3 bytes). */
int csp_pack12(const uint16_t *src, int n_samples, uint8_t *dst, int dst_max);
int csp_unpack12(const uint8_t *src, int src_len, uint16_t *dst, int n_samples);

/* Spatial / temporal compress into dst. Returns payload bytes or <0 on error. */
int csp_compress_frame(
    csp_encoder_t *enc,
    const uint16_t *raw,
    uint8_t *dst,
    int dst_max,
    int frame_index,
    int *out_type,
    int *out_quant);

int csp_decompress_frame(
    csp_encoder_t *enc,
    const uint8_t *src,
    int src_len,
    uint16_t *dst,
    int frame_type,
    int quant);

/* CBR helper: bytes per frame for target MiB/s. */
int csp_budget_bytes(int target_mibs, int fps_num, int fps_den);

#endif
