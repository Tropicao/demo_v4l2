#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "utils.h"

void dump_debug_data(const char *filename, void *base)
{
    int fd = 0;

    fd = open(filename, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR);
    if(fd < 0)
    {
        ERR("Cannot open YUV debug file");
        return;
    }
    write(fd, base, FRAME_WIDTH * FRAME_HEIGHT * 3 / 2);
    close(fd);
}

void yuv2rgb(uint8_t in[], uint8_t out[], int width, int height)
{
    int total = width * height;
    int Y, Cb = 0, Cr = 0, index = 0;
    float R, G, B;
    int x, y;


    /*NV21 is a semi planar format : 1 full plane for Y (1 pixel per Y), and one full plane for interlaced VU
     * (1 pixel per VU couple) */
    fprintf(stdout, "Start processing image with dimensions %dx%d^", width, height);
    for (y = 0; y < height; y++)
    {
        for (x = 0; x < width; x++)
        {
            Y = in[y * width + x];
            if (Y < 0) Y += 255;

            if ((x & 1) == 0)
            {
                Cr = in[(y >> 1) * (width) + x + total];
                Cb = in[(y >> 1) * (width) + x + total + 1];
                if (Cb < 0)
                    Cb += 127;
                else
                    Cb -= 128;
                if (Cr < 0)
                    Cr += 127;
                else
                    Cr -= 128;
            }

            //R = 1.164 * (Y - 16.0) + 1.596 * (Cr - 128.0);
            //G = 1.164 * (Y - 16.0) - 0.813 * (Cr - 128.0) - 0.391 * (Cb - 128.0);
            //B = 1.164 * (Y - 16.0) + 2.018 * (Cb - 128.0);
            R = Y + 1.403 * Cr;
            G = Y - 0.444 * Cb - 0.714 * Cr;
            B = Y + 1.771 * Cb;

            if (R < 0) R = 0; else if (R > 255) R = 255;
            if (G < 0) G = 0; else if (G > 255) G = 255;
            if (B < 0) B = 0; else if (B > 255) B = 255;
            out[index] = R; // Alpha
            index++;
            out[index] = G; // Alpha
            index++;
            out[index] = B; // Alpha
            index++;
        }
    }
}


