#include <stdio.h>
#include <stdlib.h>
#include <jpeglib.h>
#include <setjmp.h>

#include "utils.h"
#include "jpeg_encoder.h"

#define DEFAULT_QUALITY             50

unsigned char *jpeg_encoder_encode_frame(uint8_t *input_buf, unsigned long *output_size)
{
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPROW row_pointer[1];
    uint8_t *rgb = NULL;
    uint8_t *jpeg = NULL;
    uint8_t *tmprowbuf;

    if(!input_buf)
    {
        ERR("Cannot encode JPEG frame : input is invalid");
        return NULL;
    }

    if(!output_size)
    {
        ERR("Cannot encoder JPEG frame : output variables not properly allocated");
        return NULL;
    }

    rgb = calloc(3 * FRAME_WIDTH * FRAME_HEIGHT, sizeof(uint8_t));

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_mem_dest(&cinfo, (unsigned char **)&jpeg, output_size);
    cinfo.image_width = FRAME_WIDTH;
    cinfo.image_height = FRAME_HEIGHT;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_YCbCr;
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, DEFAULT_QUALITY, TRUE);
    jpeg_start_compress(&cinfo, TRUE);
    tmprowbuf = calloc(FRAME_SIZE*3, sizeof(uint8_t));
    row_pointer[0] = &tmprowbuf[0];
    while(cinfo.next_scanline < cinfo.image_height)
    {
        /* Inspired from https://gist.github.com/royshil/fa98604b01787172b270 */
        unsigned i, j;
        unsigned offset = cinfo.next_scanline * cinfo.image_width * 2;
        for (i = 0, j = 0; i < cinfo.image_width * 2; i += 4, j += 6)
        {
            tmprowbuf[j + 0] = input_buf[offset + i + 0];
            tmprowbuf[j + 1] = input_buf[offset + i + 1];
            tmprowbuf[j + 2] = input_buf[offset + i + 3];
            tmprowbuf[j + 3] = input_buf[offset + i + 2];
            tmprowbuf[j + 4] = input_buf[offset + i + 1];
            tmprowbuf[j + 5] = input_buf[offset + i + 3];
        }
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }
    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    if(rgb)
        free(rgb);
    if(tmprowbuf)
        free(tmprowbuf);

    INF("JPEG frame encoded");
    return jpeg;
}

