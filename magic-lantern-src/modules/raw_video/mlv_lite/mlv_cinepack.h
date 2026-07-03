#ifndef _mlv_cinepack_h_
#define _mlv_cinepack_h_

/* Self-contained CINEPACK v3 + CSP stream for mlv_lite (no weak main-binary deps). */

#define MLV_CINEPACK_MAGIC 0x4B504E43
#define MLV_CINEPACK_VERSION 3
#define MLV_CSP_MAGIC 0x31505343
#define MLV_CSP_FRAME_MAGIC 0x46505343

#define MLV_TARGET_1080P120_CENTIMBS 10000  /* 100 MB/s */
#define MLV_TARGET_4K25_CENTIMBS     8500   /* 85 MB/s */
#define MLV_TARGET_6K25_CENTIMBS     9000   /* 90 MB/s */

void mlv_cinepack_arm(int on, int quality, int res_idx, int fps_idx, int beast);
int mlv_cinepack_active(void);
int mlv_cinepack_stream_mode(void);
int mlv_cinepack_quality(void);
int mlv_cinepack_max_frame_bytes(void);
int mlv_cinepack_last_frame_bytes(void);
int mlv_cinepack_rolling_centimbs(void);

int mlv_cinepack_begin_file(void * file, int w, int h, int fps_x1000, int bpp);
int mlv_cinepack_write_frame(const void * payload, int size, int frame_num);
void mlv_cinepack_end_file(void);
int mlv_cinepack_frame_count(void);

void mlv_cinepack_set_target_centimbs(int centi_mbs);
int mlv_cinepack_compress(
    void * dst, int dst_max,
    const void * src, int src_bytes,
    int width, int height, int quality_pct);

#endif
