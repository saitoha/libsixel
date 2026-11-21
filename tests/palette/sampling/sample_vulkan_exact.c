/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/filter-sample.h"
#include "sample_vulkan_exact.h"

#if defined(HAVE_VULKAN)
/* The scalar line test is the reference for the device's integer result. */
static int
sample_line(unsigned char const *pixels, int width, int height,
    int x, int y, int dx, int dy)
{
    int worst;
    int i;
    int c;
    int delta;
    size_t center;
    size_t at;

    if (x - 2 * dx < 0 || x + 2 * dx >= width ||
        y - 2 * dy < 0 || y + 2 * dy >= height)
        return 255;
    worst = 0;
    center = ((size_t)y * width + x) * 3u;
    for (i = -2; i <= 2; ++i) {
        at = ((size_t)(y + dy * i) * width + x + dx * i) * 3u;
        for (c = 0; c < 3; ++c) {
            delta = (int)pixels[at + c] - (int)pixels[center + c];
            if (delta < 0) delta = -delta;
            if (delta > worst) worst = delta;
        }
    }
    return worst;
}
#endif

int
sample_vulkan_exact_check(int edge)
{
#if defined(HAVE_VULKAN)
    enum { WIDTH = 67, HEIGHT = 59 };
    unsigned char pixels[WIDTH * HEIGHT * 3];
    uint32_t expected[SIXEL_SAMPLE_SOLID_CELLS * 5u];
    uint32_t actual[SIXEL_SAMPLE_SOLID_CELLS * 5u];
    uint32_t state;
    unsigned int key;
    size_t at;
    size_t i;
    int x;
    int y;
    int c;
    int clip_x;
    int clip_y;
    int clip_width;
    int clip_height;
    int attempt;
    SIXELSTATUS status;

    state = 0x5392a8d1u;
    for (attempt = 0; attempt < 4; ++attempt) {
        for (i = 0; i < sizeof(pixels); ++i) {
            state = state * 1664525u + 1013904223u;
            pixels[i] = edge ? (unsigned char)(i / 3u % 13u)
                : (unsigned char)(state >> 24);
        }
        if (edge) {
            /* Horizontal and vertical thin solids cross clipped edges. */
            for (y = 0; y < HEIGHT; ++y) {
                for (x = 0; x < WIDTH; ++x) {
                    if ((y >= 8 && y < 11) || (x >= 30 && x < 33)) {
                        at = ((size_t)y * WIDTH + x) * 3u;
                        pixels[at] = 250;
                        pixels[at + 1] = 10;
                        pixels[at + 2] = 90;
                    }
                }
            }
        } else {
            /* Include a flat patch among the noisy pixels. */
            for (y = 12; y < 29; ++y) {
                for (x = 15; x < 41; ++x) {
                    at = ((size_t)y * WIDTH + x) * 3u;
                    pixels[at] = 61;
                    pixels[at + 1] = 150;
                    pixels[at + 2] = 230;
                }
            }
        }
        clip_x = attempt == 0 ? 0 : attempt;
        clip_y = attempt == 0 ? 0 : attempt + 1;
        clip_width = WIDTH - clip_x - (attempt % 2);
        clip_height = HEIGHT - clip_y - (attempt % 2);
        memset(expected, 0, sizeof(expected));
        for (y = clip_y; y < clip_y + clip_height; y += 3) {
            for (x = clip_x; x < clip_x + clip_width; x += 3) {
                if (sample_line(pixels, WIDTH, HEIGHT, x, y, 1, 0) > 6 &&
                    sample_line(pixels, WIDTH, HEIGHT, x, y, 0, 1) > 6)
                    continue;
                at = ((size_t)y * WIDTH + x) * 3u;
                key = ((unsigned int)(pixels[at] >> 4) << 8) |
                    ((unsigned int)(pixels[at + 1] >> 4) << 4) |
                    (unsigned int)(pixels[at + 2] >> 4);
                ++expected[key * 5u];
                for (c = 0; c < 3; ++c)
                    expected[key * 5u + 1u + (unsigned int)c] +=
                        pixels[at + (size_t)c];
            }
        }
        status = sixel_filter_sample_solid_gpu(pixels, WIDTH, HEIGHT,
            clip_x, clip_y, clip_width, clip_height, NULL, actual);
        if (status == SIXEL_FEATURE_ERROR) return 77;
        if (status != SIXEL_OK) {
            fprintf(stderr, "Vulkan scan failed: %d\n", status);
            return 1;
        }
        if (memcmp(actual, expected, sizeof(expected)) != 0) {
            for (i = 0; i < SIXEL_SAMPLE_SOLID_CELLS * 5u; ++i) {
                if (actual[i] != expected[i]) {
                    fprintf(stderr, "cell %lu: %u != %u\n",
                        (unsigned long)i, actual[i], expected[i]);
                    break;
                }
            }
            return 1;
        }
    }
    return 0;
#else
    (void)edge;
    return 77;
#endif
}
