#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include "sixel.h"

enum
{
   STBI_default = 0, // only used for req_comp

   STBI_grey       = 1,
   STBI_grey_alpha = 2,
   STBI_rgb        = 3,
   STBI_rgb_alpha  = 4
};

extern unsigned char *
stbi_load(char const *filename,
          int *x, int *y,
          int *comp, int req_comp);

#define        FMT_GIF            0
#define        FMT_PNG            1
#define        FMT_BMP            2
#define        FMT_JPG            3
#define        FMT_TGA            4
#define        FMT_WBMP    5
#define        FMT_TIFF    6
#define        FMT_SIXEL   7
#define        FMT_PNM            8
#define        FMT_GD2     9

static int ConvSixel(char *filename, int maxPalet, int optPalet)
{
    int n, len, max;
    FILE *fp = stdin;
    unsigned char *data;
    LibSixel_Image image;
    LibSixel_OutputContext context;
    int x, y, comp;
    int i;

    if (filename == NULL) {
        return (-1);
    }

    data = stbi_load(filename,
                     &x, &y,
                     &comp, STBI_default);

    if (data == NULL) {
        return (-1);
    }

    len = 0;
    max = 64 * 1024;

    if ( maxPalet < 2 ) {
        maxPalet = 2;
    } else if ( maxPalet > PALETTE_MAX ) {
        maxPalet = PALETTE_MAX;
    }

    image.pixels = data;
    image.sy = y;
    image.sx = x;
    image.ncolors = 256;

    for (i = 0; i < 256; i++) {
        image.red[i] = i;
        image.green[i] = i;
        image.blue[i] = i;
    }

    image.keycolor = -1;

    context.putchar = putchar;
    context.puts = puts;
    context.printf = printf;

    LibSixel_ImageToSixel(&image, &context);

    return 0;
}

int main(int ac, char *av[])
{
    int n;
    int mx = 1;
    int maxPalet = PALETTE_MAX;
    int optPalet = 0;

    for ( ; ; ) {
        while ( (n = getopt(ac, av, "p:c")) != EOF ) {
            switch(n) {
            case 'p':
                maxPalet = atoi(optarg);
                break;
            case 'c':
                optPalet = 1;
                break;
            default:
                fprintf(stderr, "Usage: %s [-p MaxPalet] [-c] <file name...>\n", av[0]);
                exit(0);
            }
        }
        if ( optind >= ac )
            break;
        av[mx++] = av[optind++];
    }

    if ( mx <= 1 ) {
        ConvSixel(NULL, maxPalet, optPalet);

    } else {
            for ( n = 1 ; n < mx ; n++ )
            ConvSixel(av[n], maxPalet, optPalet);
    }

    return 0;
}
