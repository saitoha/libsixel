
#ifndef LIBSIXEL_SIXEL_H
#define LIBSIXEL_SIXEL_H

static const int PALETTE_MAX = 1024;

typedef struct LibSixel_ImageStruct {
    /* Palette-based image pixels */
    unsigned char *pixels;
    int sx;
    int sy;
    int ncolors;
    int paletted;
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

void LibSixel_ImageToSixel(LibSixel_ImagePtr im, LibSixel_OutputContextPtr context);

#endif /* LIBSIXEL_SIXEL_H */

// EOF
