/* SPDX-License-Identifier: MIT */
/*
 * The command-line program around every step of the SIXEL codec tutorial.
 * It is deliberately ordinary code: PNM input and output, a fixed-palette
 * quantizer, timing, and a self test. Each stepN.c supplies only
 * encode_sixel() and decode_sixel().
 *
 *   stepN encode [IN.pnm]            PNM to SIXEL on standard output
 *   stepN decode [IN.six]            SIXEL to PPM on standard output
 *   stepN synth KIND W H             a synthetic test image as PPM
 *   stepN selftest                   round-trip synthetic images
 *   stepN bench IN.pnm [SCALE] [N]   time encoding and decoding
 *
 * encode and decode read standard input without a file name, so they
 * work in pipelines: ./step1 encode in.ppm | ./step1 decode > out.ppm
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "tutorial.h"

int
image_alloc(image_t *img, int width, int height)
{
    unsigned char *pixels;

    if (width <= 0 || height <= 0 ||
            width > TUTORIAL_MAX_SIDE || height > TUTORIAL_MAX_SIDE) {
        return -1;
    }
    pixels = calloc((size_t)width * (size_t)height, 1);
    if (pixels == NULL) {
        return -1;
    }
    free(img->pixels);
    img->pixels = pixels;
    img->width = width;
    img->height = height;
    return 0;
}

void
image_free(image_t *img)
{
    free(img->pixels);
    img->pixels = NULL;
}

static int
pct_to_byte(int p)
{
    if (p > 100) {
        p = 100;
    }
    return (p * 255 + 50) / 100;
}

void
set_rgb_pct(image_t *img, int color, int r, int g, int b)
{
    img->palette[color * 3 + 0] = (unsigned char)pct_to_byte(r);
    img->palette[color * 3 + 1] = (unsigned char)pct_to_byte(g);
    img->palette[color * 3 + 2] = (unsigned char)pct_to_byte(b);
}

static double
hls_channel(double m1, double m2, double h)
{
    h -= 360.0 * (int)(h / 360.0);
    if (h < 60.0) {
        return m1 + (m2 - m1) * h / 60.0;
    }
    if (h < 180.0) {
        return m2;
    }
    if (h < 240.0) {
        return m1 + (m2 - m1) * (240.0 - h) / 60.0;
    }
    return m1;
}

/* SIXEL HLS puts blue at 0 degrees, red at 120 and green at 240, so the
 * usual red-at-zero hue is rotated by 240 degrees. */
void
set_hls(image_t *img, int color, int h, int l, int s)
{
    double lum = (l > 100 ? 100 : l) / 100.0;
    double sat = (s > 100 ? 100 : s) / 100.0;
    double m2 = lum <= 0.5 ? lum * (1.0 + sat) : lum + sat - lum * sat;
    double m1 = 2.0 * lum - m2;
    double hue = (double)((h + 240) % 360);
    double rgb[3];
    int i;

    rgb[0] = hls_channel(m1, m2, hue + 120.0);
    rgb[1] = hls_channel(m1, m2, hue);
    rgb[2] = hls_channel(m1, m2, hue + 240.0);
    for (i = 0; i < 3; i++) {
        img->palette[color * 3 + i] = (unsigned char)(rgb[i] * 255.0 + 0.5);
    }
}

int
parse_params(unsigned char const **pp, unsigned char const *end,
             int *params, int max)
{
    unsigned char const *p = *pp;
    int n = 0;
    int value;
    int i;

    for (;;) {
        value = 0;
        while (p < end && *p >= '0' && *p <= '9') {
            if (value < 1000000) {
                value = value * 10 + (*p - '0');
            }
            p++;
        }
        if (n < max) {
            params[n] = value;
        }
        n++;
        if (p < end && *p == ';') {
            p++;
        } else {
            break;
        }
    }
    for (i = n; i < max; i++) {
        params[i] = 0;
    }
    *pp = p;
    return n < max ? n : max;
}

void
buf_reserve(buffer_t *b, size_t extra)
{
    size_t capacity;
    unsigned char *data;

    if (b->capacity - b->size >= extra) {
        return;
    }
    capacity = b->capacity ? b->capacity : 4096;
    while (capacity - b->size < extra) {
        capacity *= 2;
    }
    data = realloc(b->data, capacity);
    if (data == NULL) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    b->data = data;
    b->capacity = capacity;
}

void
buf_printf(buffer_t *b, char const *fmt, ...)
{
    va_list ap;
    int n;

    buf_reserve(b, 64);
    va_start(ap, fmt);
    n = vsnprintf((char *)b->data + b->size, b->capacity - b->size, fmt, ap);
    va_end(ap);
    if (n >= 0 && (size_t)n >= b->capacity - b->size) {
        buf_reserve(b, (size_t)n + 1);
        va_start(ap, fmt);
        n = vsnprintf((char *)b->data + b->size, b->capacity - b->size,
                      fmt, ap);
        va_end(ap);
    }
    if (n > 0) {
        b->size += (size_t)n;
    }
}

void
buf_free(buffer_t *b)
{
    free(b->data);
    b->data = NULL;
    b->size = b->capacity = 0;
}

void
put_header_and_palette(image_t const *img, buffer_t *out)
{
    int c;

    buf_printf(out, "\033Pq\"1;1;%d;%d", img->width, img->height);
    for (c = 0; c < img->ncolors; c++) {
        buf_printf(out, "#%d;2;%d;%d;%d", c,
                   PCT(img->palette[c * 3 + 0]),
                   PCT(img->palette[c * 3 + 1]),
                   PCT(img->palette[c * 3 + 2]));
    }
}

/* ------------------------------------------------------------------ */
/* PNM input and output, and conversion to an indexed image.           */

/* Read a whole file, or standard input when path is NULL or "-". */
static int
read_file(char const *path, buffer_t *b)
{
    int from_stdin = path == NULL || strcmp(path, "-") == 0;
    FILE *fp = from_stdin ? stdin : fopen(path, "rb");
    size_t n;

    if (fp == NULL) {
        perror(path);
        return -1;
    }
    for (;;) {
        buf_reserve(b, 65536);
        n = fread(b->data + b->size, 1, b->capacity - b->size, fp);
        if (n == 0) {
            break;
        }
        b->size += n;
    }
    if (!from_stdin) {
        fclose(fp);
    }
    return 0;
}

/* Write to standard output. Binary PNM data would garble a terminal, so
 * it is refused there; SIXEL is meant to be shown there. */
static int
write_stdout(unsigned char const *data, size_t size, int binary)
{
    if (binary && isatty(STDOUT_FILENO)) {
        fprintf(stderr, "not writing a binary image to a terminal; "
                "redirect the output, e.g. > out.ppm\n");
        return -1;
    }
    if (fwrite(data, 1, size, stdout) != size || fflush(stdout) != 0) {
        perror("stdout");
        return -1;
    }
    return 0;
}

static int
pnm_token(unsigned char const **p, unsigned char const *end)
{
    int value = 0;

    for (;;) {
        while (*p < end && (**p == ' ' || **p == '\t' ||
                            **p == '\r' || **p == '\n')) {
            (*p)++;
        }
        if (*p < end && **p == '#') {
            while (*p < end && **p != '\n') {
                (*p)++;
            }
            continue;
        }
        break;
    }
    if (*p >= end || **p < '0' || **p > '9') {
        return -1;
    }
    while (*p < end && **p >= '0' && **p <= '9') {
        if (value < 1000000) {
            value = value * 10 + (**p - '0');
        }
        (*p)++;
    }
    return value;
}

/* Parse a P2, P3, P5 or P6 file with a maxval of at most 255. The samples
 * are returned as one byte each, in file order: width * height * channels
 * bytes, where channels is 1 for gray and 3 for color. For P5 and P6 that
 * is the file's pixel data itself, copied without any conversion. */
static unsigned char *
parse_pnm(buffer_t const *file, char const *name, int *width, int *height,
          int *channels, int *maxval)
{
    unsigned char const *p = file->data;
    unsigned char const *end = p + file->size;
    unsigned char *samples;
    size_t i;
    size_t n;
    int kind;
    int v;

    if (file->size < 2 || p[0] != 'P' || p[1] < '2' || p[1] > '6' ||
            p[1] == '4') {
        fprintf(stderr, "%s: not a P2, P3, P5 or P6 PNM file\n", name);
        return NULL;
    }
    kind = p[1] - '0';
    *channels = kind == 3 || kind == 6 ? 3 : 1;
    p += 2;
    *width = pnm_token(&p, end);
    *height = pnm_token(&p, end);
    *maxval = pnm_token(&p, end);
    if (*width <= 0 || *height <= 0 || *width > TUTORIAL_MAX_SIDE ||
            *height > TUTORIAL_MAX_SIDE || *maxval <= 0 || *maxval > 255) {
        fprintf(stderr, "%s: unsupported size or maxval (at most 255)\n",
                name);
        return NULL;
    }
    n = (size_t)*width * (size_t)*height * (size_t)*channels;
    samples = malloc(n);
    if (samples == NULL) {
        return NULL;
    }
    if (kind >= 5) {
        p++;                        /* the single whitespace byte */
        if ((size_t)(end - p) < n) {
            goto truncated;
        }
        memcpy(samples, p, n);
    } else {
        for (i = 0; i < n; i++) {
            v = pnm_token(&p, end);
            if (v < 0) {
                goto truncated;
            }
            samples[i] = (unsigned char)(v > *maxval ? *maxval : v);
        }
    }
    return samples;
truncated:
    fprintf(stderr, "%s: truncated\n", name);
    free(samples);
    return NULL;
}

/* Write P6: the palette color of every pixel, row by row. */
static int
write_ppm(image_t const *img)
{
    buffer_t b = { 0 };
    size_t i;
    size_t n = (size_t)img->width * (size_t)img->height;
    int rc;

    buf_printf(&b, "P6\n%d %d\n255\n", img->width, img->height);
    buf_reserve(&b, n * 3);
    for (i = 0; i < n; i++) {
        memcpy(b.data + b.size, img->palette + img->pixels[i] * 3, 3);
        b.size += 3;
    }
    rc = write_stdout(b.data, b.size, 1);
    buf_free(&b);
    return rc;
}

/* A gray image is already indexed: each gray level 0..maxval is the
 * palette index of a register holding that gray, so the samples are the
 * pixels unchanged. */
static void
index_gray(unsigned char *samples, int width, int height, int maxval,
           image_t *img)
{
    int c;

    free(img->pixels);
    img->pixels = samples;
    img->width = width;
    img->height = height;
    img->ncolors = maxval + 1;
    for (c = 0; c <= maxval; c++) {
        memset(img->palette + c * 3, (c * 255 + maxval / 2) / maxval, 3);
    }
}

/* Use the exact colors when there are at most 256 of them. Otherwise map
 * to a 6x6x6 color cube with 4x4 ordered dithering. Palette construction
 * is a separate subject; this keeps the tutorial focused on SIXEL itself. */
static void
index_rgb(unsigned char *rgb, int width, int height, int maxval,
          image_t *img)
{
    static int const bayer[16] = {
        0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5
    };
    size_t n = (size_t)width * (size_t)height;
    size_t i;
    int c;
    int ch;
    int x;
    int y;

    if (maxval != 255) {
        for (i = 0; i < n * 3; i++) {
            rgb[i] = (unsigned char)((rgb[i] * 255 + maxval / 2) / maxval);
        }
    }
    image_alloc(img, width, height);
    img->ncolors = 0;
    for (i = 0; i < n; i++) {
        for (c = 0; c < img->ncolors; c++) {
            if (memcmp(img->palette + c * 3, rgb + i * 3, 3) == 0) {
                break;
            }
        }
        if (c == img->ncolors) {
            if (c == 256) {
                break;
            }
            memcpy(img->palette + c * 3, rgb + i * 3, 3);
            img->ncolors++;
        }
        img->pixels[i] = (unsigned char)c;
    }
    if (i == n) {
        free(rgb);
        return;
    }
    img->ncolors = 216;
    for (c = 0; c < 216; c++) {
        img->palette[c * 3 + 0] = (unsigned char)(c / 36 * 51);
        img->palette[c * 3 + 1] = (unsigned char)(c / 6 % 6 * 51);
        img->palette[c * 3 + 2] = (unsigned char)(c % 6 * 51);
    }
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            int t = bayer[(y & 3) * 4 + (x & 3)];
            int level[3];

            i = (size_t)y * width + x;
            for (ch = 0; ch < 3; ch++) {
                /* 51 values between cube levels, threshold in 1/16 steps */
                level[ch] = (rgb[i * 3 + ch] * 16 + t * 51) / (51 * 16);
                if (level[ch] > 5) {
                    level[ch] = 5;
                }
            }
            img->pixels[i] = (unsigned char)(level[0] * 36 + level[1] * 6 +
                                             level[2]);
        }
    }
    free(rgb);
}

/* Read a PNM file (or standard input) into an indexed image, optionally
 * enlarged `scale` times with nearest-neighbor sampling for benchmarks. */
static int
load_indexed(char const *path, int scale, image_t *img)
{
    buffer_t file = { 0 };
    unsigned char *samples;
    unsigned char *big;
    size_t bytes;
    int width;
    int height;
    int channels;
    int maxval;
    int x;
    int y;

    if (read_file(path, &file) != 0) {
        return -1;
    }
    samples = parse_pnm(&file, path ? path : "stdin", &width, &height,
                        &channels, &maxval);
    buf_free(&file);
    if (samples == NULL) {
        return -1;
    }
    if (scale > 1) {
        if ((size_t)width * scale > TUTORIAL_MAX_SIDE ||
                (size_t)height * scale > TUTORIAL_MAX_SIDE) {
            free(samples);
            return -1;
        }
        bytes = (size_t)channels;
        big = malloc((size_t)width * scale * (size_t)height * scale * bytes);
        if (big == NULL) {
            free(samples);
            return -1;
        }
        for (y = 0; y < height * scale; y++) {
            for (x = 0; x < width * scale; x++) {
                memcpy(big + ((size_t)y * width * scale + x) * bytes,
                       samples + ((size_t)(y / scale) * width + x / scale) *
                       bytes, bytes);
            }
        }
        free(samples);
        samples = big;
        width *= scale;
        height *= scale;
    }
    if (channels == 1) {
        index_gray(samples, width, height, maxval, img);
    } else {
        index_rgb(samples, width, height, maxval, img);
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Synthetic images and the self test.                                 */

static unsigned int
next_random(unsigned int *state)
{
    *state = *state * 1103515245u + 12345u;
    return *state >> 16;
}

static int
synth(char const *kind, int width, int height, image_t *img)
{
    unsigned int seed = 1;
    int x;
    int y;
    int c;

    if (image_alloc(img, width, height) != 0) {
        return -1;
    }
    img->ncolors = 256;
    for (c = 0; c < 256; c++) {
        img->palette[c * 3 + 0] = (unsigned char)c;
        img->palette[c * 3 + 1] = (unsigned char)(c * 7);
        img->palette[c * 3 + 2] = (unsigned char)(255 - c);
    }
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            unsigned char *px = img->pixels + (size_t)y * width + x;

            if (strcmp(kind, "noise") == 0) {
                *px = (unsigned char)(next_random(&seed) & 255);
            } else if (strcmp(kind, "flat") == 0) {
                *px = 0;
                img->ncolors = 1;
            } else if (strcmp(kind, "two") == 0) {
                *px = (unsigned char)((x / 3 + y / 2) & 1);
                img->ncolors = 2;
            } else if (strcmp(kind, "sparse") == 0) {
                /* islands of colors separated by long gaps */
                *px = (unsigned char)((x % 23 < 2 && y % 5 != 0) ?
                                      1 + (x / 23 + y) % 7 : 0);
                img->ncolors = 8;
            } else if (strcmp(kind, "gradient") == 0) {
                *px = (unsigned char)((x * 256 / width +
                                       (next_random(&seed) & 3)) & 255);
            } else {
                fprintf(stderr, "unknown kind: %s\n", kind);
                return -1;
            }
        }
    }
    return 0;
}

/* Encode src, decode the result and compare. Unless quiet, explain the
 * first problem found, down to the first pixel that differs. */
static int
roundtrip(image_t const *src, char const *label, int quiet)
{
    buffer_t six = { 0 };
    image_t dst = { 0 };
    size_t i;
    size_t n = (size_t)src->width * (size_t)src->height;
    int rc = 1;

    if (encode_sixel(src, &six) != 0) {
        if (!quiet) {
            fprintf(stderr, "%s: encode failed\n", label);
        }
        goto end;
    }
    if (decode_sixel(six.data, six.size, &dst) != 0) {
        if (!quiet) {
            fprintf(stderr, "%s: decode failed\n", label);
        }
        goto end;
    }
    if (dst.width != src->width || dst.height != src->height) {
        if (!quiet) {
            fprintf(stderr, "%s: decoded size is %dx%d\n", label,
                    dst.width, dst.height);
        }
        goto end;
    }
    for (i = 0; i < n && dst.pixels[i] == src->pixels[i]; i++) {
    }
    if (i < n) {
        if (!quiet) {
            fprintf(stderr, "%s: pixel (%d, %d) is register %d, "
                    "expected %d\n", label, (int)(i % src->width),
                    (int)(i / src->width), dst.pixels[i], src->pixels[i]);
        }
        goto end;
    }
    rc = 0;
end:
    buf_free(&six);
    image_free(&dst);
    return rc;
}

static int
selftest(void)
{
    static char const *kinds[] = { "noise", "flat", "two", "sparse",
                                   "gradient" };
    static int const sizes[][2] = {
        { 1, 1 }, { 1, 7 }, { 7, 1 }, { 5, 6 }, { 63, 11 }, { 64, 12 },
        { 65, 13 }, { 130, 37 }, { 301, 200 }, { 1000, 61 }
    };
    char label[64];
    image_t img = { 0 };
    size_t k;
    size_t s;
    int failures = 0;
    int tests = 0;

    for (k = 0; k < sizeof(kinds) / sizeof(kinds[0]); k++) {
        for (s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
            snprintf(label, sizeof(label), "%s %dx%d", kinds[k],
                     sizes[s][0], sizes[s][1]);
            if (synth(kinds[k], sizes[s][0], sizes[s][1], &img) != 0) {
                return 1;
            }
            failures += roundtrip(&img, label, failures >= 3);
            tests++;
        }
    }
    image_free(&img);
    printf("%d of %d round trips passed\n", tests - failures, tests);
    return failures != 0;
}

static double
now(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static int
bench(char const *path, int scale, int iterations)
{
    image_t img = { 0 };
    image_t dst = { 0 };
    buffer_t six = { 0 };
    double t0;
    double enc = 1e30;
    double dec = 1e30;
    double mp;
    int i;

    if (load_indexed(path, scale, &img) != 0) {
        return 1;
    }
    for (i = 0; i < iterations; i++) {
        six.size = 0;
        t0 = now();
        encode_sixel(&img, &six);
        t0 = now() - t0;
        enc = t0 < enc ? t0 : enc;
    }
    for (i = 0; i < iterations; i++) {
        t0 = now();
        decode_sixel(six.data, six.size, &dst);
        t0 = now() - t0;
        dec = t0 < dec ? t0 : dec;
    }
    mp = (double)img.width * img.height / 1e6;
    printf("%dx%d, %d colors, %zu bytes: encode %.2f ms (%.0f Mpx/s), "
           "decode %.2f ms (%.0f Mpx/s)\n", img.width, img.height,
           img.ncolors, six.size, enc * 1e3, mp / enc, dec * 1e3, mp / dec);
    image_free(&img);
    image_free(&dst);
    buf_free(&six);
    return 0;
}

static int
usage(char const *argv0)
{
    fprintf(stderr,
            "usage: %s encode [IN.pnm]      PNM to SIXEL on stdout\n"
            "       %s decode [IN.six]      SIXEL to PPM on stdout\n"
            "       %s synth KIND W H       test image as PPM on stdout\n"
            "       %s selftest             round-trip test images\n"
            "       %s bench IN.pnm [SCALE] [N]\n"
            "Without a file name, or with -, encode and decode read stdin.\n"
            "KIND is noise, flat, two, sparse or gradient.\n",
            argv0, argv0, argv0, argv0, argv0);
    return 2;
}

int
main(int argc, char **argv)
{
    image_t img = { 0 };
    buffer_t buf = { 0 };
    buffer_t six = { 0 };
    char const *cmd = argc > 1 ? argv[1] : "";
    int rc = 1;

    if (strcmp(cmd, "encode") == 0 && argc <= 3) {
        if (load_indexed(argc == 3 ? argv[2] : NULL, 1, &img) == 0 &&
                encode_sixel(&img, &six) == 0) {
            rc = write_stdout(six.data, six.size, 0) != 0;
        }
    } else if (strcmp(cmd, "decode") == 0 && argc <= 3) {
        if (read_file(argc == 3 ? argv[2] : NULL, &buf) == 0 &&
                decode_sixel(buf.data, buf.size, &img) == 0) {
            rc = write_ppm(&img) != 0;
        } else if (buf.data != NULL) {
            fprintf(stderr, "decode failed\n");
        }
    } else if (strcmp(cmd, "synth") == 0 && argc == 5) {
        if (synth(argv[2], atoi(argv[3]), atoi(argv[4]), &img) == 0) {
            rc = write_ppm(&img) != 0;
        }
    } else if (strcmp(cmd, "selftest") == 0 && argc == 2) {
        rc = selftest();
    } else if (strcmp(cmd, "bench") == 0 && argc >= 3) {
        rc = bench(argv[2], argc > 3 ? atoi(argv[3]) : 1,
                   argc > 4 ? atoi(argv[4]) : 5);
    } else {
        rc = usage(argv[0]);
    }
    image_free(&img);
    buf_free(&buf);
    buf_free(&six);
    return rc;
}
