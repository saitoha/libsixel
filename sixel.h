
#ifndef LIBSIXEL_SIXEL_H
#define LIBSIXEL_SIXEL_H

static const int PALETTE_MAX = 256;

typedef struct LibSixel_ImageStruct {
    /* Palette-based image pixels */
    unsigned char *pixels;
    int sx;
    int sy;
    int depth;
    int ncolors;
    int red[PALETTE_MAX];
    int green[PALETTE_MAX];
    int blue[PALETTE_MAX];
    int keycolor;  /* background color */
} LibSixel_Image, *LibSixel_ImagePtr;

typedef int (* putchar_function)(int ch);
typedef int (* puts_function)(const char *str);
typedef int (* printf_function)(const char *fmt, ...);

typedef struct LibSixel_OutputContextStruct {
    putchar_function putchar;
    puts_function puts;
    printf_function printf;
} LibSixel_OutputContext, *LibSixel_OutputContextPtr;

#ifdef __cplusplus
extern "C" {
#endif

extern LibSixel_ImagePtr
LibSixel_Image_create(int sx, int sy, int depth, int ncolors);

extern void
LibSixel_Image_setpalette(LibSixel_ImagePtr im,
                          int n, int r, int g, int b);

extern void
LibSixel_Image_setpixels(LibSixel_ImagePtr im, uint8_t *pixels);

extern void
LibSixel_Image_copy(LibSixel_ImagePtr dst,
                    LibSixel_ImagePtr src, int w, int h);

extern void
LibSixel_Image_destroy(LibSixel_ImagePtr im);

extern void
LibSixel_Image_fill(LibSixel_ImagePtr im, int color);

extern void
LibSixel_Image_fillrectangle(LibSixel_ImagePtr im,
                             int x1, int y1, int x2, int y2,
                             int color);

extern void
LibSixel_Image_setpixel(LibSixel_ImagePtr im,
                        int x, int y, int color);

extern void
LibSixel_ImageToSixel(LibSixel_ImagePtr im,
                      LibSixel_OutputContextPtr context);

extern LibSixel_ImagePtr
LibSixel_SixelToImage(uint8_t *p, int len);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_SIXEL_H */

/* EOF */
