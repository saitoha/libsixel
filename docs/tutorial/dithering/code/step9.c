/* SPDX-License-Identifier: MIT */
/*
 * Step 9: step 3 with faster ways to find the nearest color.
 *
 *   step9 IN PAL [scan|cache|memo|grid][,stats]
 *
 * scan   compare with every color, as before
 * cache  remember the answer of recent colors; exact
 * memo   remember one answer per cell of 32 x 32 x 32; not exact
 * grid   keep a short list of possible colors per cell of 16 x 16 x 16; exact
 *
 * With stats, the program also reports the work it did to standard error.
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tutorial.h"

int const dither_keeps_tone = DITHER_KEEPS_NUMBERS;

enum { METHOD_SCAN, METHOD_CACHE, METHOD_MEMO, METHOD_GRID };
enum {
    CACHE_SIZE = 1 << 16,
    MEMO_SIZE = 32 * 32 * 32,
    CELLS = 16 * 16 * 16,
    NOT_YET = -1
};

typedef struct lookup {
    palette_t const *pal;
    int method;
    uint32_t *cache_key;        /* color + 1, so that 0 means empty */
    unsigned char *cache_index;
    short *memo;
    int *cell_start;            /* where a cell's list starts, or NOT_YET */
    short *cell_count;
    unsigned char *pool;        /* all the lists */
    long pool_used;
    long queries;
    long compared;
} lookup_t;

static long
distance2(unsigned char const *color, int r, int g, int b)
{
    long dr = r - color[0];
    long dg = g - color[1];
    long db = b - color[2];

    return dr * dr + dg * dg + db * db;
}

static int
scan(lookup_t *t, int r, int g, int b)
{
    t->compared += t->pal->ncolors;
    return nearest_color(t->pal, r, g, b);
}

static int
cache_lookup(lookup_t *t, int r, int g, int b)
{
    uint32_t color = (uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)b;
    uint32_t slot = (color * 2654435761u) >> 16;
    int index;

    if (t->cache_key[slot] == color + 1) {
        return t->cache_index[slot];
    }
    index = scan(t, r, g, b);
    t->cache_key[slot] = color + 1;
    t->cache_index[slot] = (unsigned char)index;
    return index;
}

/* The first color to enter a cell decides the answer for all its colors. */
static int
memo_lookup(lookup_t *t, int r, int g, int b)
{
    int cell = (r >> 3) << 10 | (g >> 3) << 5 | (b >> 3);

    if (t->memo[cell] == NOT_YET) {
        t->memo[cell] = (short)scan(t, r, g, b);
    }
    return t->memo[cell];
}

/* List every color that can be the nearest for some color of this cell.
 * Let n be the nearest color to the cell's center c, at distance d, and let
 * every color of the cell lie within r of c. A color q of the cell is within
 * d + r of n, so its nearest color p is too, and then p is within d + 2r of
 * c. Any color farther than that can be left out. The list keeps the order
 * of the palette, so a tie goes to the same color as in a full scan. */
static void
build_cell(lookup_t *t, int cell)
{
    palette_t const *pal = t->pal;
    double const r = 13.0;          /* half the diagonal of 15 x 15 x 15 */
    double cr = (cell >> 8) * 16 + 7.5;
    double cg = (cell >> 4 & 15) * 16 + 7.5;
    double cb = (cell & 15) * 16 + 7.5;
    double best = 1e30;
    double limit;
    double dr;
    double dg;
    double db;
    double d;
    int count = 0;
    int i;

    for (i = 0; i < pal->ncolors; i++) {
        dr = cr - pal->rgb[i][0];
        dg = cg - pal->rgb[i][1];
        db = cb - pal->rgb[i][2];
        d = dr * dr + dg * dg + db * db;
        best = d < best ? d : best;
    }
    limit = sqrt(best) + 2.0 * r;
    limit *= limit;
    t->cell_start[cell] = (int)t->pool_used;
    for (i = 0; i < pal->ncolors; i++) {
        dr = cr - pal->rgb[i][0];
        dg = cg - pal->rgb[i][1];
        db = cb - pal->rgb[i][2];
        if (dr * dr + dg * dg + db * db <= limit) {
            t->pool[t->pool_used++] = (unsigned char)i;
            count++;
        }
    }
    t->cell_count[cell] = (short)count;
}

static int
grid_lookup(lookup_t *t, int r, int g, int b)
{
    int cell = (r >> 4) << 8 | (g >> 4) << 4 | (b >> 4);
    unsigned char const *list;
    long best = 1L << 40;
    long d;
    int index = 0;
    int i;

    if (t->cell_start[cell] == NOT_YET) {
        build_cell(t, cell);
    }
    list = t->pool + t->cell_start[cell];
    t->compared += t->cell_count[cell];
    for (i = 0; i < t->cell_count[cell]; i++) {
        d = distance2(t->pal->rgb[list[i]], r, g, b);
        if (d < best) {
            best = d;
            index = list[i];
        }
    }
    return index;
}

static int
find_color(lookup_t *t, int r, int g, int b)
{
    t->queries++;
    switch (t->method) {
    case METHOD_CACHE:
        return cache_lookup(t, r, g, b);
    case METHOD_MEMO:
        return memo_lookup(t, r, g, b);
    case METHOD_GRID:
        return grid_lookup(t, r, g, b);
    default:
        return scan(t, r, g, b);
    }
}

static void
lookup_free(lookup_t *t)
{
    free(t->cache_key);
    free(t->cache_index);
    free(t->memo);
    free(t->cell_start);
    free(t->cell_count);
    free(t->pool);
}

static int
lookup_init(lookup_t *t, palette_t const *pal, int method)
{
    int i;

    memset(t, 0, sizeof(*t));
    t->pal = pal;
    t->method = method;
    if (method == METHOD_CACHE) {
        t->cache_key = calloc(CACHE_SIZE, sizeof(*t->cache_key));
        t->cache_index = malloc(CACHE_SIZE);
        return t->cache_key == NULL || t->cache_index == NULL;
    }
    if (method == METHOD_MEMO) {
        t->memo = malloc(MEMO_SIZE * sizeof(*t->memo));
        if (t->memo == NULL) {
            return 1;
        }
        for (i = 0; i < MEMO_SIZE; i++) {
            t->memo[i] = NOT_YET;
        }
    }
    if (method == METHOD_GRID) {
        t->cell_start = malloc(CELLS * sizeof(*t->cell_start));
        t->cell_count = malloc(CELLS * sizeof(*t->cell_count));
        t->pool = malloc((size_t)CELLS * (size_t)pal->ncolors);
        if (t->cell_start == NULL || t->cell_count == NULL ||
                t->pool == NULL) {
            return 1;
        }
        for (i = 0; i < CELLS; i++) {
            t->cell_start[i] = NOT_YET;
        }
    }
    return 0;
}

static int
parse_option(char const *option, int *method, int *stats)
{
    static char const *const names[] = {"scan", "cache", "memo", "grid"};
    size_t length;
    int i;

    *method = METHOD_SCAN;
    *stats = 0;
    if (option == NULL) {
        return 0;
    }
    if (strcmp(option, "stats") == 0) {
        *stats = 1;
        return 0;
    }
    length = strcspn(option, ",");
    if (option[length] == ',') {
        if (strcmp(option + length + 1, "stats") != 0) {
            return 1;
        }
        *stats = 1;
    }
    for (i = 0; i < 4; i++) {
        if (strlen(names[i]) == length &&
                strncmp(option, names[i], length) == 0) {
            *method = i;
            return 0;
        }
    }
    return 1;
}

static float
clamp(float value)
{
    return value < 0.0f ? 0.0f : value > 255.0f ? 255.0f : value;
}

int
dither(image_t *out, image_t const *in, palette_t const *pal,
       char const *option)
{
    int w = in->width;
    size_t row_size = ((size_t)w + 2) * 3 * sizeof(float);
    lookup_t table;
    float *this_row;
    float *next_row;
    float *swap;
    unsigned char const *p = in->rgb;
    float want;
    float error;
    int method;
    int stats;
    int index;
    int rgb[3];
    int x;
    int y;
    int c;
    int k;

    if (parse_option(option, &method, &stats) != 0) {
        return 1;
    }
    this_row = calloc(1, row_size);
    next_row = calloc(1, row_size);
    if (this_row == NULL || next_row == NULL ||
            lookup_init(&table, pal, method) != 0) {
        free(this_row);
        free(next_row);
        return 1;
    }
    for (y = 0; y < in->height; y++) {
        for (x = 0; x < w; x++, p += 3) {
            k = (x + 1) * 3;
            for (c = 0; c < 3; c++) {
                rgb[c] = (int)(clamp(p[c] + this_row[k + c]) + 0.5f);
            }
            index = find_color(&table, rgb[0], rgb[1], rgb[2]);
            put_color(out, y * w + x, pal, index);
            for (c = 0; c < 3; c++) {
                want = clamp(p[c] + this_row[k + c]);
                error = want - (float)pal->rgb[index][c];
                this_row[k + 3 + c] += error * 7.0f / 16.0f;
                next_row[k - 3 + c] += error * 3.0f / 16.0f;
                next_row[k + c] += error * 5.0f / 16.0f;
                next_row[k + 3 + c] += error * 1.0f / 16.0f;
            }
        }
        swap = this_row;
        this_row = next_row;
        next_row = swap;
        memset(next_row, 0, row_size);
    }
    if (stats) {
        fprintf(stderr, "%.1f colors compared per pixel\n",
                (double)table.compared / (double)table.queries);
    }
    lookup_free(&table);
    free(this_row);
    free(next_row);
    return 0;
}
