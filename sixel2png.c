#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include "sixel.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

enum
{
   STBI_default = 0, /* only used for req_comp */
   STBI_grey = 1,
   STBI_grey_alpha = 2,
   STBI_rgb = 3,
   STBI_rgb_alpha = 4
};

extern uint8_t *
stbi_load(char const *filename, int *x, int *y, int *comp, int req_comp);

extern void
stbi_image_free(void *retval_from_stbi_load);

extern uint8_t *
make_palette(uint8_t *data, int x, int y, int n, int c);

extern uint8_t *
apply_palette(uint8_t *data,
              int width, int height, int depth,
              uint8_t *palette, int ncolors);

static int
sixel_to_png(const char *input, const char *output)
{
    uint8_t *data;
    LibSixel_ImagePtr im;
    LibSixel_OutputContext context = { putchar, puts, printf };
    int sx, sy, comp;
    int len;
    int i;
    int max;
    int n;
    FILE *fp;

    if (input != NULL && (fp = fopen(input, "r")) == NULL) {
        return (-1);
    }

    len = 0;
    max = 64 * 1024;

    if ((data = (uint8_t *)malloc(max)) == NULL) {
        return (-1);
    }

    for (;;) {
        if ((max - len) < 4096) {
            max *= 2;
            if ((data = (uint8_t *)realloc(data, max)) == NULL)
                return (-1);
        }
        if ((n = fread(data + len, 1, 4096, fp)) <= 0)
            break;
        len += n;
    }

    if (fp != stdout)
            fclose(fp);

    im = LibSixel_SixelToImage(data, len);
    if (!im) {
      return 1;
    }
    stbi_write_png(output, im->sx, im->sy,
                   STBI_rgb, im->pixels, im->sx * 3);

    LibSixel_Image_destroy(im);

    return 0;
}

int main(int argc, char *argv[])
{
    int n;
    int filecount = 1;
    char *output = strdup("/dev/stdout");
    char *input = strdup("/dev/stdin");
    const char *usage = "Usage: %s -i input.sixel -o output.png\n"
                        "       %s < input.sixel > output.png\n";

    for (;;) {
        while ((n = getopt(argc, argv, "o:i:")) != EOF) {
            switch(n) {
            case 'i':
                free(input);
                input = strdup(optarg);
                break;
            case 'o':
                free(output);
                output = strdup(optarg);
                break;
            default:
                fprintf(stderr, usage, argv[0], argv[0]);
                exit(0);
            }
        }
        if (optind >= argc) {
            break;
        }
        optind++;
    }

    sixel_to_png(input, output);

    return 0;
}

/* emacs, -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set expandtab ts=4 : */
/* EOF */
