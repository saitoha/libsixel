
#ifndef LIBSIXEL_SIXEL_H
#define LIBSIXEL_SIXEL_H

#define PALETTE_MAX 256

/* LSImage definition */
typedef struct LSImageStruct {
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
} LSImage, *LSImagePtr;

typedef int (* putchar_function)(int ch);
typedef int (* puts_function)(const char *str);
typedef int (* printf_function)(const char *fmt, ...);

typedef struct LSOutputContextStruct {
    putchar_function fn_putchar;
    puts_function fn_puts;
    printf_function fn_printf;
} LSOutputContext, *LSOutputContextPtr;

/* converter API */

#ifdef __cplusplus
extern "C" {
#endif

extern LSImagePtr LibSixel_SixelToLSImage(uint8_t *p, int len);
extern void LibSixel_LSImageToSixel(LSImagePtr im, LSOutputContextPtr context);

#ifdef __cplusplus
}
#endif

/* LSImage manipulation API */

#ifdef __cplusplus
extern "C" {
#endif

extern LSImagePtr
LSImage_create(int sx, int sy, int depth, int ncolors);

extern void
LSImage_destroy(LSImagePtr im);

extern void
LSImage_setpalette(LSImagePtr im, int n, int r, int g, int b);

extern void
LSImage_setpixels(LSImagePtr im, uint8_t *pixels);

extern void
LSImage_setpixel(LSImagePtr im, int x, int y, int color);

extern void
LSImage_copy(LSImagePtr dst, LSImagePtr src, int w, int h);

extern void
LSImage_fill(LSImagePtr im, int color);

extern void
LSImage_fillrectangle(LSImagePtr im, int x1, int y1, int x2, int y2, int color);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_SIXEL_H */

/* emacs, -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set expandtab ts=4 : */
/* EOF */
