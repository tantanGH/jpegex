#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "himem.h"
#include "picojpeg.h"
#include "jpeg_decode.h"

#define GVRAM ((uint16_t*)0xC00000)

//
//  picojpeg callback function
//
static uint8_t pjpeg_need_bytes_callback(uint8_t* pBuf, uint8_t buf_size, uint8_t* pBytes_actually_read, void* pCallback_data) {

  uint32_t* jpeg_buffer_meta = (uint32_t*)pCallback_data;
  uint8_t* jpeg_buffer = (uint8_t*)(jpeg_buffer_meta[0]);
  uint32_t jpeg_buffer_bytes = jpeg_buffer_meta[1];
  uint32_t jpeg_buffer_ofs = jpeg_buffer_meta[2];

  uint32_t remain = jpeg_buffer_bytes - jpeg_buffer_ofs;
  uint32_t n = buf_size < remain ? buf_size : remain;  
  if (n > 0) memcpy(pBuf, jpeg_buffer + jpeg_buffer_ofs, n);

  *pBytes_actually_read = (unsigned char)n;
  jpeg_buffer_meta[2] += n;

  return 0;
}

//
//  init JPEG decode handler
//
int32_t jpeg_decode_init(JPEG_DECODE_HANDLE* jpeg, int16_t brightness, int16_t half_size, int16_t extended_graphic) {

  int32_t rc = -1;

  jpeg->brightness = brightness;
  jpeg->half_size = half_size;
  jpeg->extended_graphic = extended_graphic;

  jpeg->rgb555_r = NULL;
  jpeg->rgb555_g = NULL;
  jpeg->rgb555_b = NULL;

  jpeg->rgb555_r = (uint16_t*)himem_malloc(sizeof(uint16_t) * 256, 0);
  jpeg->rgb555_g = (uint16_t*)himem_malloc(sizeof(uint16_t) * 256, 0);
  jpeg->rgb555_b = (uint16_t*)himem_malloc(sizeof(uint16_t) * 256, 0);

  if (jpeg->rgb555_r == NULL || jpeg->rgb555_g == NULL || jpeg->rgb555_b == NULL) goto exit;

  for (int16_t i = 0; i < 256; i++) {
    uint32_t c = (uint32_t)(i * 32 * brightness / 100) >> 8;
    jpeg->rgb555_r[i] = (uint16_t)((c <<  6) + 1);
    jpeg->rgb555_g[i] = (uint16_t)((c << 11) + 1);
    jpeg->rgb555_b[i] = (uint16_t)((c <<  1) + 1);
  }

  rc = 0;

exit:
  return rc;
}

//
//  close JPEG decode handler
//
void jpeg_decode_close(JPEG_DECODE_HANDLE* jpeg) {
  if (jpeg->rgb555_r != NULL) {
    himem_free(jpeg->rgb555_r, 0);
    jpeg->rgb555_r = NULL;
  }
  if (jpeg->rgb555_g != NULL) {
    himem_free(jpeg->rgb555_g, 0);
    jpeg->rgb555_g = NULL;
  }
  if (jpeg->rgb555_b != NULL) {
    himem_free(jpeg->rgb555_b, 0);
    jpeg->rgb555_b = NULL;
  }   
}

//
//  half size decode
//
static int32_t jpeg_decode_exec_half(JPEG_DECODE_HANDLE* jpeg, uint8_t* jpeg_buffer, size_t jpeg_buffer_bytes) {

  uint32_t jpeg_buffer_meta[3];
  jpeg_buffer_meta[0] = (uint32_t)jpeg_buffer;
  jpeg_buffer_meta[1] = jpeg_buffer_bytes;
  jpeg_buffer_meta[2] = 0;

  pjpeg_image_info_t image_info = { 0 };
  int32_t rc = pjpeg_decode_init(&image_info, pjpeg_need_bytes_callback, jpeg_buffer_meta, 0);
  if (rc != 0) {
    goto exit;
  }

  int32_t ofs_y = ( 512 - image_info.m_height / 2 ) / 2;
  int32_t ofs_x = jpeg->extended_graphic ? ( 768 - image_info.m_width / 2 ) / 2 : ( 512 - image_info.m_width / 2 ) / 2;

  int32_t mcu_y = 0;
  int32_t mcu_x = 0;

  int32_t pitch = jpeg->extended_graphic ? 1024 : 512;
  int32_t xmax = jpeg->extended_graphic ? 767 : 511;

  for (;;) {

    uint8_t status = pjpeg_decode_mcu();
    if (status == PJPG_NO_MORE_BLOCKS) {
      break;
    } else if (status != 0) {
      rc = status;
      goto exit;
    }
    if (mcu_y >= image_info.m_MCUSPerCol) {
      goto exit;
    }

    for (int32_t y = 0; y < image_info.m_MCUHeight; y += 8) {

      const int by_limit = min(8, image_info.m_height - (mcu_y * image_info.m_MCUHeight + y));

      for (int32_t x = 0; x < image_info.m_MCUWidth; x += 8) {

        uint8_t src_ofs = (x * 8U) + (y * 16U);
        const uint8_t *pSrcR = image_info.m_pMCUBufR + src_ofs;
        const uint8_t *pSrcG = image_info.m_pMCUBufG + src_ofs;
        const uint8_t *pSrcB = image_info.m_pMCUBufB + src_ofs;

        const int16_t bx_limit = min(8, image_info.m_width - (mcu_x * image_info.m_MCUWidth + x));

        for (int16_t by = 0; by < by_limit; by++) {

          if (by & 0x0001) {
            pSrcR += 8;
            pSrcG += 8;
            pSrcB += 8;
            continue;
          }

          int32_t gy = ofs_y + (mcu_y * image_info.m_MCUHeight + y + by)/2;
          if (gy < 0 || gy > 511) {
            pSrcR += 8;
            pSrcG += 8;
            pSrcB += 8;
            continue;
          }

          uint16_t* gvram = &(GVRAM[ gy * pitch ]);
          int32_t gx = ofs_x + (mcu_x * image_info.m_MCUWidth + x)/2;
          
          for (int16_t bx = 0; bx < bx_limit; bx++) {
            if (bx & 0x0001) {
              pSrcR++;
              pSrcG++;
              pSrcB++;
              continue;
            }
            if (gx < 0 || gx > xmax) {
              pSrcR++;
              pSrcG++;
              pSrcB++;
            } else {
              uint8_t r = *pSrcR++;
              uint8_t g = *pSrcG++;
              uint8_t b = *pSrcB++;
              gvram[ gx ] = jpeg->rgb555_g[ g ] | jpeg->rgb555_r[ r ] | jpeg->rgb555_b[ b ] | 1;
            }
            gx++;
          }

          pSrcR += (8 - bx_limit);
          pSrcG += (8 - bx_limit);
          pSrcB += (8 - bx_limit);
        }
      }
    }

    if (++mcu_x == image_info.m_MCUSPerRow) {
      mcu_x = 0;
      mcu_y++;
    }

  }

  rc = 0;

exit:

  return rc;
}

//
//  normal size decode
//
int32_t jpeg_decode_exec(JPEG_DECODE_HANDLE* jpeg, uint8_t* jpeg_buffer, size_t jpeg_buffer_bytes) {

  if (jpeg->half_size) {
    return jpeg_decode_exec_half(jpeg, jpeg_buffer, jpeg_buffer_bytes);
  }

  uint32_t jpeg_buffer_meta[3];
  jpeg_buffer_meta[0] = (uint32_t)jpeg_buffer;
  jpeg_buffer_meta[1] = jpeg_buffer_bytes;
  jpeg_buffer_meta[2] = 0;

  pjpeg_image_info_t image_info = { 0 };
  int32_t rc = pjpeg_decode_init(&image_info, pjpeg_need_bytes_callback, jpeg_buffer_meta, 0);
  if (rc != 0) {
    goto exit;
  }

  int32_t ofs_y = ( 512 - image_info.m_height ) / 2;
  int32_t ofs_x = jpeg->extended_graphic ? ( 768 - image_info.m_width ) / 2 : ( 512 - image_info.m_width ) / 2;

  int32_t mcu_y = 0;
  int32_t mcu_x = 0;

  int32_t pitch = jpeg->extended_graphic ? 1024 : 512;
  int32_t xmax = jpeg->extended_graphic ? 767 : 511;

  for (;;) {

    uint8_t status = pjpeg_decode_mcu();
    if (status == PJPG_NO_MORE_BLOCKS) {
      break;
    } else if (status != 0) {
      rc = status;
      goto exit;
    }
    if (mcu_y >= image_info.m_MCUSPerCol) {
      goto exit;
    }

    for (int32_t y = 0; y < image_info.m_MCUHeight; y += 8) {

      const int by_limit = min(8, image_info.m_height - (mcu_y * image_info.m_MCUHeight + y));

      for (int32_t x = 0; x < image_info.m_MCUWidth; x += 8) {

        uint8_t src_ofs = (x * 8U) + (y * 16U);
        const uint8_t* pSrcR = image_info.m_pMCUBufR + src_ofs;
        const uint8_t* pSrcG = image_info.m_pMCUBufG + src_ofs;
        const uint8_t* pSrcB = image_info.m_pMCUBufB + src_ofs;

        const int16_t bx_limit = min(8, image_info.m_width - (mcu_x * image_info.m_MCUWidth + x));

        for (int16_t by = 0; by < by_limit; by++) {

          int32_t gy = ofs_y + mcu_y * image_info.m_MCUHeight + y + by;
          if (gy < 0 || gy > 511) {
            pSrcR += 8;
            pSrcG += 8;
            pSrcB += 8;
            continue;
          }

          uint16_t* gvram = &(GVRAM[ gy * pitch ]);
          int32_t gx = ofs_x + mcu_x * image_info.m_MCUWidth + x;

          for (int16_t bx = 0; bx < bx_limit; bx++) {
            if (gx < 0 || gx > xmax) {
              pSrcR++;
              pSrcG++;
              pSrcB++;
            } else {
              uint8_t r = *pSrcR++;
              uint8_t g = *pSrcG++;
              uint8_t b = *pSrcB++;
              gvram[ gx ] = jpeg->rgb555_g[ g ] | jpeg->rgb555_r[ r ] | jpeg->rgb555_b[ b ] | 1;
            }
            gx++;
          }

          pSrcR += (8 - bx_limit);
          pSrcG += (8 - bx_limit);
          pSrcB += (8 - bx_limit);
        }
      }
    }

    if (++mcu_x == image_info.m_MCUSPerRow) {
      mcu_x = 0;
      mcu_y++;
    }

  }

  rc = 0;

exit:

  return rc;
}