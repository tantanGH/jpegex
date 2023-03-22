#ifndef __H_JPEG_DECODE__
#define __H_JPEG_DECODE__

#include <stdint.h>
#include <stddef.h>

typedef struct {
  int16_t brightness;
  int16_t half_size;
  int16_t extended_graphic;
  uint16_t* rgb555_r;
  uint16_t* rgb555_g;
  uint16_t* rgb555_b;
} JPEG_DECODE_HANDLE;

int32_t jpeg_decode_init(JPEG_DECODE_HANDLE* jpeg, int16_t brightness, int16_t half_size, int16_t extended_graphic);
void jpeg_decode_close(JPEG_DECODE_HANDLE* jpeg);
int32_t jpeg_decode_exec(JPEG_DECODE_HANDLE* jpeg, uint8_t* jpeg_buffer, size_t jpeg_buffer_len);

#endif