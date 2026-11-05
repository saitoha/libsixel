/* SPDX-License-Identifier: MIT */
/*
 * Step 4: the same loop with the kernel in a table, and two scan orders.
 *
 *   step4 IN PAL [KERNEL][,serpentine]
 *
 * KERNEL is one of fs (the default), atkinson, jajuni, stucki or sierra.
 * With serpentine, odd rows run from right to left and mirror the kernel.
 */
#include <stdlib.h>
#include <string.h>

#include "tutorial.h"

int const dither_keeps_tone = 1;

typedef struct tap {
    int dx;
    int dy;
    int weight;
} tap_t;

typedef struct kernel {
    char const *name;
    int divisor;
    int ntaps;
    tap_t taps[12];
} kernel_t;

static kernel_t const kernels[] = {
    {"fs", 16, 4, {{1, 0, 7}, {-1, 1, 3}, {0, 1, 5}, {1, 1, 1}}},
    {"atkinson", 8, 6, {{1, 0, 1}, {2, 0, 1}, {-1, 1, 1}, {0, 1, 1},
                        {1, 1, 1}, {0, 2, 1}}},
    {"jajuni", 48, 12, {{1, 0, 7}, {2, 0, 5}, {-2, 1, 3}, {-1, 1, 5},
                        {0, 1, 7}, {1, 1, 5}, {2, 1, 3}, {-2, 2, 1},
                        {-1, 2, 3}, {0, 2, 5}, {1, 2, 3}, {2, 2, 1}}},
    {"stucki", 42, 12, {{1, 0, 8}, {2, 0, 4}, {-2, 1, 2}, {-1, 1, 4},
                        {0, 1, 8}, {1, 1, 4}, {2, 1, 2}, {-2, 2, 1},
                        {-1, 2, 2}, {0, 2, 4}, {1, 2, 2}, {2, 2, 1}}},
    {"sierra", 32, 10, {{1, 0, 5}, {2, 0, 3}, {-2, 1, 2}, {-1, 1, 4},
                        {0, 1, 5}, {1, 1, 4}, {2, 1, 2}, {-1, 2, 2},
                        {0, 2, 3}, {1, 2, 2}}},
};

static float
clamp(float value)
{
    return value < 0.0f ? 0.0f : value > 255.0f ? 255.0f : value;
}

/* Fill in the kernel and the scan order, or return nonzero. */
static int
parse_option(char const *option, kernel_t const **kernel, int *serpentine)
{
    size_t length;
    size_t i;

    *kernel = &kernels[0];
    *serpentine = 0;
    if (option == NULL) {
        return 0;
    }
    if (strcmp(option, "serpentine") == 0) {
        *serpentine = 1;
        return 0;
    }
    length = strcspn(option, ",");
    if (option[length] == ',') {
        if (strcmp(option + length + 1, "serpentine") != 0) {
            return 1;
        }
        *serpentine = 1;
    }
    for (i = 0; i < sizeof(kernels) / sizeof(kernels[0]); i++) {
        if (strlen(kernels[i].name) == length &&
                strncmp(option, kernels[i].name, length) == 0) {
            *kernel = &kernels[i];
            return 0;
        }
    }
    return 1;
}

int
dither(image_t *out, image_t const *in, palette_t const *pal,
       char const *option)
{
    enum { MARGIN = 2, ROWS = 3 };
    int w = in->width;
    kernel_t const *kernel;
    int serpentine;
    size_t row_size = ((size_t)w + 2 * MARGIN) * 3 * sizeof(float);
    float *rows[ROWS];
    float *swap;
    float want[3];
    float error;
    unsigned char const *p;
    tap_t const *tap;
    int index;
    int reverse;
    int x;
    int y;
    int i;
    int c;
    int t;
    int k;
    int target;

    if (parse_option(option, &kernel, &serpentine) != 0) {
        return 1;
    }
    for (i = 0; i < ROWS; i++) {
        rows[i] = calloc(1, row_size);
    }
    if (rows[0] == NULL || rows[1] == NULL || rows[2] == NULL) {
        for (i = 0; i < ROWS; i++) {
            free(rows[i]);
        }
        return 1;
    }
    for (y = 0; y < in->height; y++) {
        reverse = serpentine && (y & 1);
        for (i = 0; i < w; i++) {
            x = reverse ? w - 1 - i : i;
            p = in->rgb + ((size_t)y * (size_t)w + (size_t)x) * 3;
            k = (x + MARGIN) * 3;
            for (c = 0; c < 3; c++) {
                want[c] = clamp(p[c] + rows[0][k + c]);
            }
            index = nearest_color(pal, (int)(want[0] + 0.5f),
                                  (int)(want[1] + 0.5f),
                                  (int)(want[2] + 0.5f));
            put_color(out, y * w + x, pal, index);
            for (c = 0; c < 3; c++) {
                error = want[c] - (float)pal->rgb[index][c];
                for (t = 0; t < kernel->ntaps; t++) {
                    tap = &kernel->taps[t];
                    target = (x + (reverse ? -tap->dx : tap->dx) + MARGIN) * 3;
                    rows[tap->dy][target + c] +=
                        error * (float)tap->weight / (float)kernel->divisor;
                }
            }
        }
        swap = rows[0];
        rows[0] = rows[1];
        rows[1] = rows[2];
        rows[2] = swap;
        memset(rows[2], 0, row_size);
    }
    for (i = 0; i < ROWS; i++) {
        free(rows[i]);
    }
    return 0;
}
