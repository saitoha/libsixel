#include <stdlib.h>
#include "sixel.h"
#include "config.h"

LibSixel_ImagePtr
LibSixel_Image_create(int sx, int sy, int depth, int ncolors)
{
    LibSixel_ImagePtr im;
   
    im = (LibSixel_ImagePtr)malloc(sizeof(LibSixel_Image));
    im->pixels = (uint8_t *)malloc(sx * sy * depth);
    im->sx = sx;
    im->sy = sy;
    im->depth = depth;
    im->ncolors = ncolors;
    im->keycolor = -1;
    return im;
}

void
LibSixel_Image_setpalette(LibSixel_ImagePtr im,
                          int n, int r, int g, int b)
{
    im->red[n] = r;
    im->green[n] = g;
    im->blue[n] = b;
}

void
LibSixel_Image_setpixels(LibSixel_ImagePtr im, uint8_t *pixels)
{
    im->pixels = pixels;
}

void
LibSixel_Image_copy(LibSixel_ImagePtr dst,
                    LibSixel_ImagePtr src,
                    int w, int h)
{
    int x, y;

    for (y = 0; y < h; y++) {
        for (x = 0; x < w * dst->depth; x++) {
            dst->pixels[dst->sx * dst->depth * y + x] 
                = src->pixels[src->sx * src->depth * y + x];
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
            LibSixel_Image_setpixel(im, x, y, color);
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
            LibSixel_Image_setpixel(im, x, y, color);
        }
    }
}

void
LibSixel_Image_setpixel(LibSixel_ImagePtr im, int x, int y, int color)
{
    im->pixels[im->sx * 3 * y + x * 3 + 0] = (color >> 16) & 0xff;
    im->pixels[im->sx * 3 * y + x * 3 + 1] = (color >> 8) & 0xff;
    im->pixels[im->sx * 3 * y + x * 3 + 2] = color & 0xff;
}

