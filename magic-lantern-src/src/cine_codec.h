#ifndef _cine_codec_h_
#define _cine_codec_h_

/* CINEPACK v3 — compromised stream for direct-to-card recording.
 * Records .CSP on card; PC: tools/cinepack_to_mlv.py → MLV / ProRes */

#define CINEPACK_MAGIC 0x4B504E43  /* 'CNPK' */
#define CINEPACK_VERSION 3

#define CINEPACK_FMT_MLV_LJ92  0
#define CINEPACK_FMT_CINEPACK  1

/* Bitrate targets (centi-MB/s) — hard caps for continuous card fill */
#define CINE_TARGET_1080P120_CENTIMBS 10000  /* 100 MB/s */
#define CINE_TARGET_4K25_CENTIMBS     8500   /* 85 MB/s */
#define CINE_TARGET_6K25_CENTIMBS     9000   /* 90 MB/s */

int cine_codec_enabled(void);
int cine_codec_stream_mode(void);
int cine_codec_quality(void);
int cine_codec_sensor_pct(void);
int cine_codec_lv_pct(void);
int cine_codec_target_centimbs(void);
int cine_codec_max_frame_bytes(void);
int cine_codec_last_frame_bytes(void);
int cine_codec_last_ratio_pct(void);
int cine_codec_rolling_centimbs(void);

void cine_codec_set_mode(int use_cinepack, int quality_pct);
void cine_codec_set_sensor_pct(int pct);
void cine_codec_set_lv_pct(int pct);
void cine_codec_set_record_profile(int res_idx, int fps_idx, int beast_mode);
void cine_codec_reset_governor(void);
int cine_codec_lv_scale_index(void);
int cine_codec_readout_pct_index(void);

int cinepack_compress(
    void * dst, int dst_max,
    const void * src, int src_bytes,
    int width, int height, int quality_pct);

#endif
