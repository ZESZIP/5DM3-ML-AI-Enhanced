#ifndef _cine_stream_h_
#define _cine_stream_h_

#define CIX_MAGIC 0x31584943  /* 'CIX1' */
#define CIX_FRAME_MAGIC 0x46584943  /* 'CIXF' */

int cine_stream_active(void);
int cine_stream_begin_file(void * file, int width, int height, int fps_x1000, int bpp);
void cine_stream_end(void);
int cine_stream_write_frame(const void * payload, int size, int frame_num);
int cine_stream_frame_count(void);
int64_t cine_stream_bytes_written(void);

#endif
