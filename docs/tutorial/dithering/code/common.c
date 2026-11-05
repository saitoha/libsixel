/* SPDX-License-Identifier: MIT */
/*
 * Pictures and palettes: everything the lessons use around dither().
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tutorial.h"

/* ---- pictures ------------------------------------------------------ */

int
image_new(image_t *img, int width, int height)
{
    img->width = width;
    img->height = height;
    img->rgb = malloc((size_t)width * (size_t)height * 3);
    return img->rgb == NULL;
}

void
image_free(image_t *img)
{
    free(img->rgb);
    img->rgb = NULL;
}

/* The next number of a PNM header, skipping white space and # comments. */
static int
read_number(FILE *f, int *value)
{
    int c = getc(f);

    for (;;) {
        while (c != EOF && isspace(c)) {
            c = getc(f);
        }
        if (c != '#') {
            break;
        }
        while (c != EOF && c != '\n') {
            c = getc(f);
        }
    }
    if (c == EOF || !isdigit(c)) {
        return 1;
    }
    *value = 0;
    while (c != EOF && isdigit(c)) {
        if (*value > 100000000) {
            return 1;
        }
        *value = *value * 10 + (c - '0');
        c = getc(f);
    }
    if (c != EOF) {
        ungetc(c, f);
    }
    return 0;
}

static int
read_sample(FILE *f, int text, int max, int *value)
{
    int c;

    if (text) {
        if (read_number(f, value) != 0) {
            return 1;
        }
    } else {
        c = getc(f);
        if (c == EOF) {
            return 1;
        }
        *value = c;
    }
    if (max != 255) {
        *value = (*value * 255 + max / 2) / max;
    }
    return *value > 255;
}

int
image_read(image_t *img, char const *path)
{
    FILE *f = path != NULL ? fopen(path, "rb") : stdin;
    char magic[3] = "";
    int w = 0;
    int h = 0;
    int max = 0;
    int kind;
    int text;
    int gray;
    int c;
    int v;
    size_t i;
    size_t n;

    if (f == NULL) {
        perror(path);
        return 1;
    }
    if (fread(magic, 1, 2, f) != 2 || magic[0] != 'P' ||
            read_number(f, &w) != 0 || read_number(f, &h) != 0 ||
            read_number(f, &max) != 0 || w < 1 || h < 1 ||
            w > 32768 || h > 32768 || max < 1 || max > 255) {
        fprintf(stderr, "%s: not an 8-bit PNM picture\n",
                path != NULL ? path : "input");
        goto fail;
    }
    kind = magic[1] - '0';
    if (kind != 2 && kind != 3 && kind != 5 && kind != 6) {
        fprintf(stderr, "%s: not a P2, P3, P5 or P6 picture\n",
                path != NULL ? path : "input");
        goto fail;
    }
    text = kind == 2 || kind == 3;
    gray = kind == 2 || kind == 5;
    if (!text) {
        getc(f);                    /* the one white space after the header */
    }
    if (image_new(img, w, h) != 0) {
        goto fail;
    }
    n = (size_t)w * (size_t)h;
    for (i = 0; i < n; i++) {
        for (c = 0; c < (gray ? 1 : 3); c++) {
            if (read_sample(f, text, max, &v) != 0) {
                fprintf(stderr, "%s: picture is cut short\n",
                        path != NULL ? path : "input");
                image_free(img);
                goto fail;
            }
            img->rgb[i * 3 + (size_t)c] = (unsigned char)v;
        }
        if (gray) {
            img->rgb[i * 3 + 1] = img->rgb[i * 3];
            img->rgb[i * 3 + 2] = img->rgb[i * 3];
        }
    }
    if (path != NULL) {
        fclose(f);
    }
    return 0;
fail:
    if (path != NULL) {
        fclose(f);
    }
    return 1;
}

int
image_write(image_t const *img)
{
    size_t n = (size_t)img->width * (size_t)img->height * 3;

    printf("P6\n%d %d\n255\n", img->width, img->height);
    return fwrite(img->rgb, 1, n, stdout) != n || fflush(stdout) != 0;
}

/* ---- palettes ------------------------------------------------------ */

int
palette_from_image(palette_t *pal, image_t const *img)
{
    long n = (long)img->width * img->height;

    if (n > 256) {
        fprintf(stderr, "a palette has at most 256 colors, not %ld\n", n);
        return 1;
    }
    pal->ncolors = (int)n;
    memcpy(pal->rgb, img->rgb, (size_t)n * 3);
    return 0;
}

int
palette_read(palette_t *pal, char const *path)
{
    image_t img;
    int result;

    if (image_read(&img, path) != 0) {
        return 1;
    }
    result = palette_from_image(pal, &img);
    image_free(&img);
    return result;
}

void
palette_cube(palette_t *pal, int levels)
{
    int r;
    int g;
    int b;
    int n = 0;
    int top = levels - 1;

    for (r = 0; r < levels; r++) {
        for (g = 0; g < levels; g++) {
            for (b = 0; b < levels; b++) {
                pal->rgb[n][0] = (unsigned char)((r * 255 + top / 2) / top);
                pal->rgb[n][1] = (unsigned char)((g * 255 + top / 2) / top);
                pal->rgb[n][2] = (unsigned char)((b * 255 + top / 2) / top);
                n++;
            }
        }
    }
    pal->ncolors = n;
}

void
palette_gray(palette_t *pal, int levels)
{
    int i;
    int top = levels - 1;

    for (i = 0; i < levels; i++) {
        memset(pal->rgb[i], (i * 255 + top / 2) / top, 3);
    }
    pal->ncolors = levels;
}

/* Lesson 1 shows this function: compare with every color of the palette. */
int
nearest_color(palette_t const *pal, int r, int g, int b)
{
    int best = 0;
    long best_distance = 1L << 40;
    long distance;
    int dr;
    int dg;
    int db;
    int i;

    for (i = 0; i < pal->ncolors; i++) {
        dr = r - pal->rgb[i][0];
        dg = g - pal->rgb[i][1];
        db = b - pal->rgb[i][2];
        distance = (long)dr * dr + (long)dg * dg + (long)db * db;
        if (distance < best_distance) {
            best_distance = distance;
            best = i;
        }
    }
    return best;
}

/* For each color, the largest channel difference to its closest neighbor;
 * their average. A cube of n levels gives exactly its step, 255 / (n - 1). */
double
palette_gap(palette_t const *pal)
{
    double sum = 0.0;
    int i;
    int j;
    int c;
    int d;
    int far;
    int best;

    if (pal->ncolors < 2) {
        return 0.0;
    }
    for (i = 0; i < pal->ncolors; i++) {
        best = 256;
        for (j = 0; j < pal->ncolors; j++) {
            if (j == i) {
                continue;
            }
            far = 0;
            for (c = 0; c < 3; c++) {
                d = abs(pal->rgb[i][c] - pal->rgb[j][c]);
                far = d > far ? d : far;
            }
            best = far < best ? far : best;
        }
        sum += best;
    }
    return sum / pal->ncolors;
}

void
put_color(image_t *out, int p, palette_t const *pal, int index)
{
    memcpy(out->rgb + (size_t)p * 3, pal->rgb[index], 3);
}

/* ---- synthetic pictures -------------------------------------------- */

void
image_gradient(image_t *img, int r0, int g0, int b0, int r1, int g1, int b1)
{
    int x;
    int y;
    int top = img->width > 1 ? img->width - 1 : 1;
    unsigned char *p = img->rgb;

    for (y = 0; y < img->height; y++) {
        for (x = 0; x < img->width; x++) {
            *p++ = (unsigned char)(r0 + (r1 - r0) * x / top);
            *p++ = (unsigned char)(g0 + (g1 - g0) * x / top);
            *p++ = (unsigned char)(b0 + (b1 - b0) * x / top);
        }
    }
}

void
image_flat(image_t *img, int r, int g, int b)
{
    int i;
    int n = img->width * img->height;

    for (i = 0; i < n; i++) {
        img->rgb[i * 3] = (unsigned char)r;
        img->rgb[i * 3 + 1] = (unsigned char)g;
        img->rgb[i * 3 + 2] = (unsigned char)b;
    }
}
