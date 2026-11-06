/* SPDX-License-Identifier: MIT */
/*
 * Small helpers for the dithering lessons.
 *
 *   dtool gradient W H                 the usual test gradient
 *   dtool gradient W H R G B R G B     a gradient between two colors
 *   dtool flat W H R G B               one color
 *   dtool cube LEVELS                  a palette with LEVELS per channel
 *   dtool gray LEVELS                  a palette of LEVELS grays
 *   dtool average N IN                 replace each pixel by an N x N average
 *   dtool light N IN                   the same average, taken in light
 *   dtool measure REFERENCE OUT        lesson 5's three numbers
 *   dtool seams OUT HEIGHT             lesson 10's look at band starts
 *
 * Pictures go to standard output as binary PPM.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tutorial.h"

/* A palette is a picture one pixel high. */
static int
write_palette(palette_t const *pal)
{
    image_t img;
    int result;

    if (image_new(&img, pal->ncolors, 1) != 0) {
        return 1;
    }
    memcpy(img.rgb, pal->rgb, (size_t)pal->ncolors * 3);
    result = image_write(&img);
    image_free(&img);
    return result;
}

/* The mean of a square of pixels around (x, y), clipped at the edges. */
static void
square_mean(image_t const *img, int x, int y, int n, double mean[3])
{
    int x0 = x - n / 2 < 0 ? 0 : x - n / 2;
    int y0 = y - n / 2 < 0 ? 0 : y - n / 2;
    int x1 = x0 + n > img->width ? img->width : x0 + n;
    int y1 = y0 + n > img->height ? img->height : y0 + n;
    unsigned char const *p;
    double count = (double)(x1 - x0) * (double)(y1 - y0);
    int i;
    int j;
    int c;

    mean[0] = mean[1] = mean[2] = 0.0;
    for (j = y0; j < y1; j++) {
        p = img->rgb + ((size_t)j * (size_t)img->width + (size_t)x0) * 3;
        for (i = x0; i < x1; i++, p += 3) {
            for (c = 0; c < 3; c++) {
                mean[c] += p[c];
            }
        }
    }
    for (c = 0; c < 3; c++) {
        mean[c] /= count;
    }
}

static int
average(int n, char const *path)
{
    image_t in;
    image_t out;
    double mean[3];
    unsigned char *q;
    int x;
    int y;
    int c;

    if (n < 1 || image_read(&in, path) != 0) {
        return 1;
    }
    if (image_new(&out, in.width, in.height) != 0) {
        return 1;
    }
    q = out.rgb;
    for (y = 0; y < in.height; y++) {
        for (x = 0; x < in.width; x++) {
            square_mean(&in, x, y, n, mean);
            for (c = 0; c < 3; c++) {
                *q++ = (unsigned char)(mean[c] + 0.5);
            }
        }
    }
    c = image_write(&out);
    image_free(&in);
    image_free(&out);
    return c;
}

/* The sRGB curve, from a stored number from 0 to 255 to light from 0 to 1. */
static double
to_light(double stored)
{
    double v = stored / 255.0;

    return v <= 0.04045 ? v / 12.92 : pow((v + 0.055) / 1.055, 2.4);
}

static double
to_stored(double light)
{
    return 255.0 * (light <= 0.0031308 ? light * 12.92
                    : 1.055 * pow(light, 1.0 / 2.4) - 0.055);
}

/* What an eye sees from afar: the mean of the light of each N x N square. */
static int
light_average(int n, char const *path)
{
    image_t in;
    image_t out;
    double table[256];
    double sum[3];
    unsigned char const *p;
    unsigned char *q;
    int x0;
    int y0;
    int x1;
    int y1;
    int x;
    int y;
    int i;
    int j;
    int c;

    if (n < 1 || image_read(&in, path) != 0) {
        return 1;
    }
    if (image_new(&out, in.width, in.height) != 0) {
        return 1;
    }
    for (i = 0; i < 256; i++) {
        table[i] = to_light(i);
    }
    q = out.rgb;
    for (y = 0; y < in.height; y++) {
        y0 = y - n / 2 < 0 ? 0 : y - n / 2;
        y1 = y0 + n > in.height ? in.height : y0 + n;
        for (x = 0; x < in.width; x++) {
            x0 = x - n / 2 < 0 ? 0 : x - n / 2;
            x1 = x0 + n > in.width ? in.width : x0 + n;
            sum[0] = sum[1] = sum[2] = 0.0;
            for (j = y0; j < y1; j++) {
                p = in.rgb + ((size_t)j * (size_t)in.width + (size_t)x0) * 3;
                for (i = x0; i < x1; i++, p += 3) {
                    for (c = 0; c < 3; c++) {
                        sum[c] += table[p[c]];
                    }
                }
            }
            for (c = 0; c < 3; c++) {
                *q++ = (unsigned char)(to_stored(
                    sum[c] / ((double)(x1 - x0) * (double)(y1 - y0))) + 0.5);
            }
        }
    }
    c = image_write(&out);
    image_free(&in);
    image_free(&out);
    return c;
}

/* Root mean square difference, pixel by pixel. */
static double
pixel_error(image_t const *a, image_t const *b)
{
    size_t n = (size_t)a->width * (size_t)a->height * 3;
    double sum = 0.0;
    double d;
    size_t i;

    for (i = 0; i < n; i++) {
        d = (double)a->rgb[i] - (double)b->rgb[i];
        sum += d * d;
    }
    return sqrt(sum / (double)n);
}

/* Root mean square difference of the means of 16 x 16 tiles: what remains
 * of the error when the eye averages each tile. */
static double
area_error(image_t const *a, image_t const *b)
{
    double ma[3];
    double mb[3];
    double sum = 0.0;
    double d;
    long count = 0;
    int x;
    int y;
    int c;

    for (y = 8; y < a->height + 8; y += 16) {
        for (x = 8; x < a->width + 8; x += 16) {
            square_mean(a, x, y, 16, ma);
            square_mean(b, x, y, 16, mb);
            for (c = 0; c < 3; c++) {
                d = ma[c] - mb[c];
                sum += d * d;
                count++;
            }
        }
    }
    return sqrt(sum / (double)count);
}

/* The average length, in pixels, of the runs of equal brightness in a row,
 * with brightness cut into 32 levels. Bands make long runs. */
static double
flat_runs(image_t const *img)
{
    unsigned char const *p = img->rgb;
    double runs = 0.0;
    int previous;
    int level;
    int x;
    int y;

    for (y = 0; y < img->height; y++) {
        previous = -1;
        for (x = 0; x < img->width; x++, p += 3) {
            level = (int)(0.2126 * p[0] + 0.7152 * p[1] + 0.0722 * p[2]) / 8;
            if (level != previous) {
                runs += 1.0;
                previous = level;
            }
        }
    }
    return (double)img->width * img->height / runs;
}

static int
measure(char const *reference, char const *output)
{
    image_t a;
    image_t b;

    if (image_read(&a, reference) != 0 || image_read(&b, output) != 0) {
        return 1;
    }
    if (a.width != b.width || a.height != b.height) {
        fprintf(stderr, "the pictures have different sizes\n");
        return 1;
    }
    printf("pixel error %.2f, area error %.2f, flat runs %.1f (original "
           "%.1f)\n", pixel_error(&a, &b), area_error(&a, &b), flat_runs(&b),
           flat_runs(&a));
    image_free(&a);
    image_free(&b);
    return 0;
}

/* How many times the color changes along a row, in the first rows of each
 * band and in all other rows. A dither that has not got going yet changes
 * color less often, and that difference is what a seam looks like. */
static int
seams(char const *path, int height)
{
    enum { SEAM_ROWS = 2 };
    image_t img;
    double seam = 0.0;
    double other = 0.0;
    long seam_rows = 0;
    long other_rows = 0;
    long changes;
    unsigned char const *p;
    int x;
    int y;

    if (height < 1 || image_read(&img, path) != 0) {
        return 1;
    }
    for (y = 0; y < img.height; y++) {
        p = img.rgb + (size_t)y * (size_t)img.width * 3;
        changes = 0;
        for (x = 1; x < img.width; x++, p += 3) {
            changes += memcmp(p, p + 3, 3) != 0;
        }
        /* The top of the picture starts cold in every method: not a seam. */
        if (y >= height && y % height < SEAM_ROWS) {
            seam += (double)changes;
            seam_rows++;
        } else if (y >= SEAM_ROWS) {
            other += (double)changes;
            other_rows++;
        }
    }
    printf("color changes per row: %.1f at band starts, %.1f elsewhere\n",
           seam_rows > 0 ? seam / (double)seam_rows : 0.0,
           other_rows > 0 ? other / (double)other_rows : 0.0);
    image_free(&img);
    return 0;
}

static int
usage(void)
{
    fprintf(stderr, "usage: dtool gradient W H [R G B R G B]\n"
            "       dtool flat W H R G B\n"
            "       dtool cube LEVELS\n"
            "       dtool gray LEVELS\n"
            "       dtool average N IN.ppm\n"
            "       dtool light N IN.ppm\n"
            "       dtool measure REFERENCE.ppm OUT.ppm\n"
            "       dtool seams OUT.ppm HEIGHT\n");
    return 2;
}

int
main(int argc, char **argv)
{
    image_t img;
    palette_t pal;
    int a[8];
    int i;

    if (argc < 3) {
        return usage();
    }
    if (strcmp(argv[1], "cube") == 0 || strcmp(argv[1], "gray") == 0) {
        a[0] = atoi(argv[2]);
        if (a[0] < 2 || (argv[1][0] == 'c' ? a[0] > 6 : a[0] > 256)) {
            return usage();
        }
        if (argv[1][0] == 'c') {
            palette_cube(&pal, a[0]);
        } else {
            palette_gray(&pal, a[0]);
        }
        return write_palette(&pal);
    }
    if (strcmp(argv[1], "average") == 0 && argc == 4) {
        return average(atoi(argv[2]), argv[3]);
    }
    if (strcmp(argv[1], "light") == 0 && argc == 4) {
        return light_average(atoi(argv[2]), argv[3]);
    }
    if (strcmp(argv[1], "measure") == 0 && argc == 4) {
        return measure(argv[2], argv[3]);
    }
    if (strcmp(argv[1], "seams") == 0 && argc == 4) {
        return seams(argv[2], atoi(argv[3]));
    }
    if ((strcmp(argv[1], "gradient") == 0 && (argc == 4 || argc == 10)) ||
            (strcmp(argv[1], "flat") == 0 && argc == 7)) {
        for (i = 2; i < argc; i++) {
            a[i - 2] = atoi(argv[i]);
        }
        if (a[0] < 1 || a[1] < 1 || a[0] > 32768 || a[1] > 32768 ||
                image_new(&img, a[0], a[1]) != 0) {
            return usage();
        }
        if (argv[1][0] == 'f') {
            image_flat(&img, a[2], a[3], a[4]);
        } else if (argc == 4) {
            image_gradient(&img, 32, 48, 80, 224, 192, 144);
        } else {
            image_gradient(&img, a[2], a[3], a[4], a[5], a[6], a[7]);
        }
        i = image_write(&img);
        image_free(&img);
        return i;
    }
    return usage();
}
