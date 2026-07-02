/** \file
 * CSP — direct-to-card CINEPACK stream (bypasses RAM frame ring).
 * PC: tools/cinepack_to_mlv.py → MLV or ProRes.
 */
#include "dryos.h"
#include "fio-ml.h"
#include "cine_stream.h"
#include "cine_codec.h"
#include "cinema_debug.h"

#define CSP_HDR_SIZE 128

static FILE * csp_file = 0;
static int csp_w = 0;
static int csp_h = 0;
static int csp_fps = 0;
static int csp_bpp = 14;
static int csp_frames = 0;
static int64_t csp_bytes = 0;
static int csp_active = 0;

int cine_stream_frame_count(void) { return csp_frames; }
int64_t cine_stream_bytes_written(void) { return csp_bytes; }

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

int cine_stream_active(void) { return csp_active && csp_file; }

int cine_stream_begin_file(void * file, int width, int height, int fps_x1000, int bpp)
{
    if (!file) return 0;
    csp_file = (FILE *) file;
    csp_w = width;
    csp_h = height;
    csp_fps = fps_x1000;
    csp_bpp = bpp;
    csp_frames = 0;
    csp_bytes = sizeof(csp_file_hdr_t);
    csp_active = 1;

    cine_codec_reset_governor();

    csp_file_hdr_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = CSP_MAGIC;
    hdr.version = 2;
    hdr.width = (uint16_t) width;
    hdr.height = (uint16_t) height;
    hdr.fps_x1000 = (uint32_t) fps_x1000;
    hdr.bpp = (uint8_t) bpp;
    hdr.codec = CINEPACK_VERSION;
    hdr.target_centi_mbs = (uint16_t) cine_codec_target_centimbs();

    if (FIO_WriteFile(csp_file, &hdr, sizeof(hdr)) != sizeof(hdr))
    {
        cine_debug_log("CSP header write failed");
        csp_active = 0;
        return 0;
    }

    cine_debug_log("CSP stream start %dx%d fps=%d target=%d cMB/s",
        width, height, fps_x1000, hdr.target_centi_mbs);
    return 1;
}

int cine_stream_write_frame(const void * payload, int size, int frame_num)
{
    if (!cine_stream_active() || !payload || size <= 0)
        return 0;

    csp_frame_hdr_t fh;
    fh.magic = CSP_FRAME_MAGIC;
    fh.payload_size = (uint32_t) size;
    fh.frame_number = (uint32_t) frame_num;
    fh.reserved = 0;

    if (FIO_WriteFile(csp_file, &fh, sizeof(fh)) != sizeof(fh))
        goto fail;
    if (FIO_WriteFile(csp_file, payload, size) != (uint32_t) size)
        goto fail;

    csp_frames++;
    csp_bytes += (int64_t) sizeof(fh) + size;

    if ((csp_frames % 12) == 0)
        FIO_SeekSkipFile(csp_file, 0, SEEK_CUR);

    return 1;

fail:
    cine_debug_log("CSP frame %d write fail", frame_num);
    return 0;
}

void cine_stream_end(void)
{
    if (!csp_file) return;

    if (csp_active)
    {
        csp_file_hdr_t hdr;
        memset(&hdr, 0, sizeof(hdr));
        hdr.magic = CSP_MAGIC;
        hdr.version = 2;
        hdr.width = (uint16_t) csp_w;
        hdr.height = (uint16_t) csp_h;
        hdr.fps_x1000 = (uint32_t) csp_fps;
        hdr.bpp = (uint8_t) csp_bpp;
        hdr.codec = CINEPACK_VERSION;
        hdr.target_centi_mbs = (uint16_t) cine_codec_target_centimbs();
        hdr.frame_count = (uint32_t) csp_frames;
        FIO_SeekSkipFile(csp_file, 0, SEEK_SET);
        FIO_WriteFile(csp_file, &hdr, sizeof(hdr));
        cine_debug_log("CSP stream end frames=%d bytes=%d rolling=%d cMB/s",
            csp_frames, (int) csp_bytes, cine_codec_rolling_centimbs());
    }

    csp_active = 0;
    csp_file = 0;
}
