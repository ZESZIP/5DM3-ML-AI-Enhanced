/** \file
 * CIX — direct-to-card CINEPACK stream (bypasses RAM frame ring).
 * PC tool rewrites to standard MLV.
 */
#include "dryos.h"
#include "fio-ml.h"
#include "cine_stream.h"
#include "cine_codec.h"
#include "cinema_debug.h"

#define CIX_HDR_SIZE 128

static FILE * cix_file = 0;
static int cix_w = 0;
static int cix_h = 0;
static int cix_fps = 0;
static int cix_bpp = 14;
static int cix_frames = 0;
static int64_t cix_bytes = 0;
static int cix_active = 0;

int cine_stream_frame_count(void) { return cix_frames; }
int64_t cine_stream_bytes_written(void) { return cix_bytes; }

#pragma pack(push,1)
typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t width;
    uint16_t height;
    uint32_t fps_x1000;
    uint8_t  bpp;
    uint8_t  codec;      /* 2 = CINEPACK v2 */
    uint16_t target_centi_mbs;
    uint32_t frame_count;
    uint8_t  reserved[100];
} cix_file_hdr_t;

typedef struct {
    uint32_t magic;
    uint32_t payload_size;
    uint32_t frame_number;
    uint32_t reserved;
} cix_frame_hdr_t;
#pragma pack(pop)

int cine_stream_active(void) { return cix_active && cix_file; }

int cine_stream_begin_file(void * file, int width, int height, int fps_x1000, int bpp)
{
    if (!file) return 0;
    cix_file = (FILE *) file;
    cix_w = width;
    cix_h = height;
    cix_fps = fps_x1000;
    cix_bpp = bpp;
    cix_frames = 0;
    cix_bytes = sizeof(cix_file_hdr_t);
    cix_active = 1;

    cix_file_hdr_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = CIX_MAGIC;
    hdr.version = 1;
    hdr.width = (uint16_t) width;
    hdr.height = (uint16_t) height;
    hdr.fps_x1000 = (uint32_t) fps_x1000;
    hdr.bpp = (uint8_t) bpp;
    hdr.codec = CINEPACK_VERSION;
    hdr.target_centi_mbs = (uint16_t) cine_codec_target_centimbs();

    if (FIO_WriteFile(cix_file, &hdr, sizeof(hdr)) != sizeof(hdr))
    {
        cine_debug_log("CIX header write failed");
        cix_active = 0;
        return 0;
    }

    cine_debug_log("CIX stream start %dx%d fps=%d target=%d cMB/s",
        width, height, fps_x1000, hdr.target_centi_mbs);
    return 1;
}

int cine_stream_write_frame(const void * payload, int size, int frame_num)
{
    if (!cine_stream_active() || !payload || size <= 0)
        return 0;

    cix_frame_hdr_t fh;
    fh.magic = CIX_FRAME_MAGIC;
    fh.payload_size = (uint32_t) size;
    fh.frame_number = (uint32_t) frame_num;
    fh.reserved = 0;

    if (FIO_WriteFile(cix_file, &fh, sizeof(fh)) != sizeof(fh))
        goto fail;
    if (FIO_WriteFile(cix_file, payload, size) != (uint32_t) size)
        goto fail;

    cix_frames++;
    cix_bytes += (int64_t) sizeof(fh) + size;

    if ((cix_frames % 24) == 0)
        FIO_SeekSkipFile(cix_file, 0, SEEK_CUR);

    return 1;

fail:
    cine_debug_log("CIX frame %d write fail", frame_num);
    return 0;
}

void cine_stream_end(void)
{
    if (!cix_file) return;

    if (cix_active)
    {
        cix_file_hdr_t hdr;
        memset(&hdr, 0, sizeof(hdr));
        hdr.magic = CIX_MAGIC;
        hdr.version = 1;
        hdr.width = (uint16_t) cix_w;
        hdr.height = (uint16_t) cix_h;
        hdr.fps_x1000 = (uint32_t) cix_fps;
        hdr.bpp = (uint8_t) cix_bpp;
        hdr.codec = CINEPACK_VERSION;
        hdr.frame_count = (uint32_t) cix_frames;
        FIO_SeekSkipFile(cix_file, 0, SEEK_SET);
        FIO_WriteFile(cix_file, &hdr, sizeof(hdr));
        cine_debug_log("CIX stream end frames=%d", cix_frames);
    }

    cix_active = 0;
    cix_file = 0;
}
