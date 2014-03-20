
#if !defined(LIBSIXEL_SIXEL_H)
#define LIBSIXEL_SIXEL_H 1

static const int PALETTE_MAX = 256;

typedef struct LibSixel_ImageStruct {
    /* Palette-based image pixels */
    unsigned char **pixels;
    int sx;
    int sy;
    int ncolors;
    int red[PALETTE_MAX];
    int green[PALETTE_MAX];
    int blue[PALETTE_MAX];
    int keycolor;  /* background color */
} LibSixel_Image, *LibSixel_ImagePtr;

typedef void (* putc_function)(int ch);
typedef void (* puts_function)(char *str);
typedef void (* printf_function)(char *fmt, ...);

typedef struct LibSixel_OutputContextStruct {
    putc_function putc;
    puts_function puts;
    printf_function printf;
} LibSixel_OutputContext, *LibSixel_OutputContextPtr;

void LibSixel_ImageToSixel(LibSixel_ImagePtr im, LibSixel_OutputContextPtr context);

#endif /* LIBSIXEL_SIXEL_H */

// EOF
