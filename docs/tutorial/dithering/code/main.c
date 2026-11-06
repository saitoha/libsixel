/* SPDX-License-Identifier: MIT */
/*
 * The command line of every step, around its dither() function.
 *
 *   stepN IN.ppm PALETTE.ppm [OPTION] > OUT.ppm    dither a picture
 *   stepN bench IN.ppm PALETTE.ppm [OPTION]        time it
 *   stepN selftest                                 check it
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "tutorial.h"

/* Every output pixel must be a palette color. */
static int
all_in_palette(image_t const *img, palette_t const *pal)
{
    int n = img->width * img->height;
    int i;
    int k;

    for (i = 0; i < n; i++) {
        for (k = 0; k < pal->ncolors; k++) {
            if (memcmp(img->rgb + (size_t)i * 3, pal->rgb[k], 3) == 0) {
                break;
            }
        }
        if (k == pal->ncolors) {
            return 0;
        }
    }
    return 1;
}

static int
selftest(void)
{
    static int const sizes[][2] = {
        {1, 1}, {2, 1}, {1, 2}, {3, 3}, {7, 5}, {64, 64}, {257, 3},
    };
    static int const levels[] = {2, 3, 6};
    palette_t pal;
    image_t in;
    image_t out;
    int failures = 0;
    size_t s;
    size_t l;
    double mean;
    long sum;
    int i;
    int k;

    /* Odd sizes and palettes: right size, palette colors only. */
    for (s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
        for (l = 0; l < sizeof(levels) / sizeof(levels[0]); l++) {
            palette_cube(&pal, levels[l]);
            image_new(&in, sizes[s][0], sizes[s][1]);
            image_new(&out, sizes[s][0], sizes[s][1]);
            image_gradient(&in, 32, 48, 80, 224, 192, 144);
            if (dither(&out, &in, &pal, NULL) != 0 ||
                    !all_in_palette(&out, &pal)) {
                printf("%dx%d, %d levels: a pixel is not a palette color\n",
                       sizes[s][0], sizes[s][1], levels[l]);
                failures++;
            }
            image_free(&in);
            image_free(&out);
        }
    }

    /* A step that does not keep the tone must be the plain nearest color. */
    if (!dither_keeps_tone) {
        palette_gray(&pal, 4);
        image_new(&in, 64, 8);
        image_new(&out, 64, 8);
        image_gradient(&in, 0, 0, 0, 255, 255, 255);
        dither(&out, &in, &pal, NULL);
        for (i = 0; i < 64 * 8; i++) {
            k = nearest_color(&pal, in.rgb[i * 3], in.rgb[i * 3 + 1],
                              in.rgb[i * 3 + 2]);
            if (memcmp(out.rgb + i * 3, pal.rgb[k], 3) != 0) {
                printf("pixel %d is not the nearest palette color\n", i);
                failures++;
                break;
            }
        }
        image_free(&in);
        image_free(&out);
    }

    /* A flat gray between two palette colors. */
    palette_gray(&pal, 2);
    image_new(&in, 64, 64);
    image_new(&out, 64, 64);
    image_flat(&in, 64, 64, 64);
    dither(&out, &in, &pal, NULL);
    sum = 0;
    for (i = 0; i < 64 * 64; i++) {
        sum += out.rgb[i * 3];
    }
    mean = (double)sum / (64.0 * 64.0);
    if (dither_keeps_tone == DITHER_KEEPS_NUMBERS &&
            (mean < 60.0 || mean > 68.0)) {
        printf("flat gray 64 averages %.2f\n", mean);
        failures++;
    }
    /* Gray 64 is 5.1% of white's light, which is 13 as a mean of 0 and 255. */
    if (dither_keeps_tone == DITHER_KEEPS_LIGHT &&
            (mean < 10.0 || mean > 16.0)) {
        printf("flat gray 64 should average 13 in light, not %.2f\n", mean);
        failures++;
    }
    if (!dither_keeps_tone && mean != 0.0) {
        printf("flat gray 64 should stay at the nearest color, not %.2f\n",
               mean);
        failures++;
    }
    image_free(&in);
    image_free(&out);

    if (failures == 0) {
        printf("all self tests passed\n");
    }
    return failures != 0;
}

static double
now(void)
{
    struct timespec t;

    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}

static int
bench(char const *in_path, char const *pal_path, char const *option)
{
    palette_t pal;
    image_t in;
    image_t out;
    double best = 1e30;
    double t;
    int i;

    if (palette_read(&pal, pal_path) != 0 || image_read(&in, in_path) != 0) {
        return 1;
    }
    if (image_new(&out, in.width, in.height) != 0) {
        return 1;
    }
    for (i = 0; i < 10; i++) {
        t = now();
        if (dither(&out, &in, &pal, option) != 0) {
            fprintf(stderr, "unknown option: %s\n", option);
            return 2;
        }
        t = now() - t;
        best = t < best ? t : best;
    }
    printf("%dx%d, %d colors: %.2f ms (%.1f ns per pixel)\n", in.width,
           in.height, pal.ncolors, best * 1e3,
           best * 1e9 / ((double)in.width * in.height));
    image_free(&in);
    image_free(&out);
    return 0;
}

int
main(int argc, char **argv)
{
    palette_t pal;
    image_t in;
    image_t out;
    char const *option;

    if (argc == 2 && strcmp(argv[1], "selftest") == 0) {
        return selftest();
    }
    if (argc >= 4 && argc <= 5 && strcmp(argv[1], "bench") == 0) {
        return bench(argv[2], argv[3], argc == 5 ? argv[4] : NULL);
    }
    if (argc < 3 || argc > 4) {
        fprintf(stderr, "usage: %s IN.ppm PALETTE.ppm [OPTION] > OUT.ppm\n"
                "       %s bench IN.ppm PALETTE.ppm [OPTION]\n"
                "       %s selftest\n", argv[0], argv[0], argv[0]);
        return 2;
    }
    option = argc == 4 ? argv[3] : NULL;
    if (palette_read(&pal, argv[2]) != 0 || image_read(&in, argv[1]) != 0) {
        return 1;
    }
    if (image_new(&out, in.width, in.height) != 0) {
        return 1;
    }
    if (dither(&out, &in, &pal, option) != 0) {
        fprintf(stderr, "unknown option: %s\n", option);
        return 2;
    }
    if (image_write(&out) != 0) {
        return 1;
    }
    image_free(&in);
    image_free(&out);
    return 0;
}
