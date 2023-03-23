#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <doslib.h>
#include <iocslib.h>
#include "himem.h"
#include "crtc.h"
#include "jpeg_decode.h"

#define VERSION "0.1.0 (2023/03/23)"

//
//  show help message
//
static void show_help_message() {
  printf("usage: jpegex [options] <file.jpg>\n");
  printf("options:\n");
  printf("   -v<n> ... brightness(1-100, default:100)\n");
  printf("   -c    ... clear screen\n");
  printf("   -s    ... half size\n");
  printf("   -e    ... use XEiJ extended graphic (768x512x32768)\n");
  printf("   -h    ... show help message\n");
}

//
//  main
//
int32_t main(int32_t argc, uint8_t* argv[]) {

  // default return code
  int32_t rc = -1;

  // file name
  uint8_t* file_name = NULL;

  // buffer pointer
  uint8_t* file_data = NULL;

  // file pointer
  FILE* fp = NULL;

  // mode
  int16_t brightness = 100;
  int16_t clear_screen = 0;
  int16_t half_size = 0;
  int16_t extended_graphic = 0;

  // credit
  printf("JPEGEX.X - A simple JPEG viewer for X680x0 " VERSION " by tantan\n");

  // check command line
  if (argc < 2) {
    show_help_message();
    goto exit;
  }

  // parse command lines
  for (int16_t i = 1; i < argc; i++) {
    if (argv[i][0] == '-' && strlen(argv[i]) >= 2) {
      if (argv[i][1] == 'v') {
        brightness = atoi(argv[i]+2);
        if (brightness < 1 || brightness > 100) {
          show_help_message();
          goto exit;
        }
      } else if (argv[i][1] == 'c') {
        clear_screen = 1;
      } else if (argv[i][1] == 's') {
        half_size = 1;
      } else if (argv[i][1] == 'e') {
        extended_graphic = 1;
      } else if (argv[i][1] == 'h') {
        show_help_message();
        goto exit;
      } else {
        printf("error: unknown option (%s).\n",argv[i]);
        goto exit;
      }
    } else {
      if (file_name != NULL) {
        printf("error: too many files.\n");
        goto exit;
      }
      file_name = argv[i];
    }
  }

  if (file_name == NULL) {
    show_help_message();
    goto exit;
  }
    
  // read JPEG data from file into memory buffer
  fp = fopen(file_name, "rb");
  if (fp == NULL) {
    printf("error: failed to open file: %s\n", file_name);
    goto exit;
  }

  fseek(fp, 0, SEEK_END);
  size_t file_size = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  file_data = (uint8_t*)himem_malloc(file_size, 0);
  if (file_data == NULL) {
    printf("error: out of memory.\n");
    goto exit;
  }

  size_t read_len = 0;
  do {
    size_t len = fread(file_data + read_len, 1, file_size - read_len, fp);
    if (len == 0) break;
    read_len += len;
  } while (read_len < file_size);
  fclose(fp);
  fp = NULL;

  B_SUPER(0);
  C_CUROFF();

  if (clear_screen) {
    G_CLR_ON();
    C_CLS_AL();
  }

  crtc_set_extra_mode(extended_graphic);
  if (extended_graphic) {
    struct FILLPTR fillptr = { 0, 0, 767, 511, 0 };
    FILL(&fillptr);
  }

  JPEG_DECODE_HANDLE jpeg_decode = { 0 };
  jpeg_decode_init(&jpeg_decode, brightness, half_size, extended_graphic);
  rc = jpeg_decode_exec(&jpeg_decode, file_data, file_size);
  jpeg_decode_close(&jpeg_decode);

  C_CURON();

exit:

  if (fp != NULL) {
    fclose(fp);
    fp = NULL;
  }

  if (file_data != NULL) {
    himem_free(file_data, 0);
    file_data = NULL;
  }

  return rc;
}