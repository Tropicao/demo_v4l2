#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdint.h>

#define ERR(...) do {\
    fprintf(stderr, "========== ERROR =========\n");\
    fprintf(stderr, __VA_ARGS__);\
    fprintf(stderr, "\n==========================\n");\
} while(0);

#define INF(...) do {\
    fprintf(stdout, __VA_ARGS__);\
    fprintf(stdout, "\n");\
} while(0);

#ifdef DEBUG
#define DBG(...) do {\
    fprintf(stdout, __VA_ARGS__);\
    fprintf(stdout, "\n");\
} while(0);
#else
#define DBG(...)
#endif

#define DEVICE_NAME_MAX_SIZE        64
#define DEVICE_NAME_DEFAULT         "/dev/video0"
#define OUTPUT_DIR_NAME_MAX_SIZE    64
#define FORMAT_MAX_SIZE             16
#define FRAME_WIDTH                 1280
#define FRAME_HEIGHT                720
#define FRAME_SIZE                  (FRAME_WIDTH * FRAME_HEIGHT * 2)
#define NB_BUF                      9
#define FRAME_RATE                  30
#define NB_DUMP_FRAME               10

void yuv2rgb(uint8_t in[], uint8_t out[], int width, int height);

#endif
