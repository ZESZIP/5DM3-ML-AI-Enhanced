#ifndef _cine_codec_h_
#define _cine_codec_h_

/* CINEPACK — proprietary in-camera compression for Cine AI Enhanced.
 * PC decode: tools/cinepack_to_mlv.py */

#define CINEPACK_MAGIC 0x4B504E43  /* 'CNPK' */

#define CINEPACK_FMT_MLV_LJ92  0
#define CINEPACK_FMT_CINEPACK  1

int cine_codec_enabled(void);
int cine_codec_quality(void);       /* 1-100 */
int cine_codec_sensor_pct(void);    /* 25-100 */
int cine_codec_lv_pct(void);        /* 25-100 */

void cine_codec_set_mode(int use_cinepack, int quality_pct);
void cine_codec_set_sensor_pct(int pct);
void cine_codec_set_lv_pct(int pct);
int cine_codec_lv_scale_index(void);
int cine_codec_readout_pct_index(void);

/* Compress 14-bit raw rectangle. Returns compressed bytes or 0 on fail. */
int cinepack_compress(
    void * dst, int dst_max,
    const void * src, int src_bytes,
    int width, int height, int quality_pct);

#endif
