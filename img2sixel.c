#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include "sixel.h"
#include "config.h"

enum
{
   STBI_default = 0, /* only used for req_comp */
   STBI_grey = 1,
   STBI_grey_alpha = 2,
   STBI_rgb = 3,
   STBI_rgb_alpha = 4
};

extern unsigned char *
stbi_load(char const *filename, int *x, int *y, int *comp, int req_comp);

extern void
stbi_image_free(void *retval_from_stbi_load);

extern unsigned char *
make_palette(unsigned char *data, int x, int y, int n, int c);

extern unsigned char *
apply_palette(unsigned char *data,
              int width, int height, int depth,
              unsigned char *palette, int ncolors);

static int
convert_to_sixel(char *filename, int ncolors)
{
    unsigned char *pixels = NULL;
    unsigned char *palette = NULL;
    unsigned char *data = NULL;
    LibSixel_ImagePtr im = NULL;
    LibSixel_OutputContext context = { putchar, puts, printf };
    int sx, sy, comp;
    int i;
    int nret = -1;

    if (filename == NULL) {
        return (-1);
    }

    if ( ncolors < 2 ) {
        ncolors = 2;
    } else if ( ncolors > PALETTE_MAX ) {
        ncolors = PALETTE_MAX;
    }

    pixels = stbi_load(filename, &sx, &sy, &comp, STBI_rgb);
    if (pixels == NULL) {
        return (-1);
    }
    palette = make_palette(pixels, sx, sy, 3, ncolors);
    if (!palette) {
        goto end;
    }
    im = LibSixel_Image_create(sx, sy, 1, ncolors);
    if (!im) {
        goto end;
    }
    for (i = 0; i < ncolors; i++) {
        LibSixel_Image_setpalette(im, i,
                                  palette[i * 3 + 0],
                                  palette[i * 3 + 1],
                                  palette[i * 3 + 2]);
    }
    data = apply_palette(pixels, sx, sy, 3, palette, ncolors);
    if (!data) {
        goto end;
    }
    LibSixel_Image_setpixels(im, data);
    LibSixel_ImageToSixel(im, &context);

end:
    if (pixels) {
        stbi_image_free(pixels);
    }
    if (palette) {
        free(palette);
    }
    if (im) {
        LibSixel_Image_destroy(im);
    }
    return 0;
}

int main(int argc, char *argv[])
{
    int n;
    int filecount = 1;
    int ncolors = PALETTE_MAX;

    for (;;) {
        while ((n = getopt(argc, argv, "p:")) != EOF) {
            switch(n) {
            case 'p':
                ncolors = atoi(optarg);
                break;
            default:
                fprintf(stderr, "Usage: %s [-p MaxPalet] <file name...>\n", argv[0]);
                exit(0);
            }
        }
        if (optind >= argc) {
            break;
        }
        argv[filecount++] = argv[optind++];
    }

    if (filecount <= 1) {
        convert_to_sixel("/dev/stdin", ncolors);
    } else {
        for (n = 1; n < filecount; n++) {
            convert_to_sixel(argv[n], ncolors);
        }
    }
    return 0;
}

/* EOF */
