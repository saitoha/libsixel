#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include "sixel.h"
#include "stb_image.c"
#include "quant.c"

static int
convert_to_sixel(char *filename, int ncolors)
{
    unsigned char *pixels;
    unsigned char *palette;
    unsigned char *data;
    LibSixel_Image image;
    LibSixel_OutputContext context = { putchar, puts, printf };
    int x, y, comp;
    int i;

    if (filename == NULL) {
        return (-1);
    }

    pixels = stbi_load(filename, &x, &y, &comp, STBI_rgb);

    if (pixels == NULL) {
        return (-1);
    }

    if ( ncolors < 2 ) {
        ncolors = 2;
    } else if ( ncolors > PALETTE_MAX ) {
        ncolors = PALETTE_MAX;
    }

    image.sy = y;
    image.sx = x;
    image.ncolors = ncolors;

    palette = make_palette(pixels, x, y, 3, ncolors);
    if (!palette) {
        return -1;
    }
    data = apply_palette(pixels, x, y, 3, palette, ncolors, 1);
    if (!data) {
        free(palette);
        return -1;
    }
    image.pixels = data;
    stbi_image_free(pixels);

    for (i = 0; i < ncolors; i++) {
        image.red[i] = palette[i * 3 + 0];
        image.green[i] = palette[i * 3 + 1];
        image.blue[i] = palette[i * 3 + 2];
    }
    free(palette);

    image.keycolor = -1;

    LibSixel_ImageToSixel(&image, &context);
    free(data);

    return 0;
}

int main(int argc, char *argv[])
{
    int n;
    int mx = 1;
    int ncolors = PALETTE_MAX;

    for ( ; ; ) {
        while ( (n = getopt(argc, argv, "p:")) != EOF ) {
            switch(n) {
            case 'p':
                ncolors = atoi(optarg);
                break;
            default:
                fprintf(stderr, "Usage: %s [-p MaxPalet] <file name...>\n", argv[0]);
                exit(0);
            }
        }
        if ( optind >= argc )
            break;
        argv[mx++] = argv[optind++];
    }

    if ( mx <= 1 ) {
        convert_to_sixel("/dev/stdin", ncolors);
    } else {
        for ( n = 1 ; n < mx ; n++ ) {
            convert_to_sixel(argv[n], ncolors);
	}
    }

    return 0;
}

// EOF
