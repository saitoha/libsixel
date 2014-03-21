#include <stdlib.h>
#include "sixel.h"
#include "config.h"

LibSixel_ImagePtr
LibSixel_Image_create(int sx, int sy)
{
    LibSixel_ImagePtr im;
   
    im = (LibSixel_ImagePtr)malloc(sizeof(LibSixel_Image));
    im->pixels = (uint8_t *)malloc(sx * sy);
    im->sx = sx;
    im->sy = sy;
    return im;
}

void
LibSixel_Image_copy(LibSixel_ImagePtr dst,
                    LibSixel_ImagePtr src,
                    int w, int h)
{
    int x, y;

    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            dst->pixels[dst->sx * y + x] = src->pixels[src->sx * y + x];
        }
    }
}

void
LibSixel_Image_destroy(LibSixel_ImagePtr im)
{
    free(im->pixels);
    free(im);
}

void
LibSixel_Image_fill(LibSixel_ImagePtr im, int color)
{
    int x, y;
    for (y = 0; y < im->sy; y++) {
        for (x = 0; x < im->sx; x++) {
            im->pixels[im->sx * y + x] = color;
        }
    }
}

void
LibSixel_Image_fillrectangle(LibSixel_ImagePtr im,
                             int x1, int y1, int x2, int y2,
                             int color)
{
    int x, y;
    for (y = y1; y < y2; y++) {
        for (x = x1; x < x2; x++) {
            im->pixels[im->sx * y + x] = color;
        }
    }
}

void
LibSixel_Image_setpixel(LibSixel_ImagePtr im, int x, int y, int color)
{
    im->pixels[im->sx * y + x] = color;
}

