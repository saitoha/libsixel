/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/gpu-palette.h"

/* One invocation selects one geometric palette family. The scalar integer
 * oracle is independent of the GPU seed grid, bounds and adjacency walk. */
int
test_delaunay_gpu_geometry(int argc, char **argv)
{
    static unsigned char pixels[2048 * 3];
    static sixel_index_t result[2048];
    unsigned char palette[256 * 3];
    sixel_gpu_palette_request_t request;
    sixel_gpu_palette_test_hooks_t hooks;
    unsigned int state;
    int kind, n, i, c, q, gap, distance, minimum, expected;

    if (argc != 2) return EXIT_FAILURE;
    kind = atoi(argv[1]);
    if (kind < 0 || kind > 5) return EXIT_FAILURE;
    n = kind == 5 ? 8 : 256;
    state = 81371;
    memset(&request, 0, sizeof(request));
    memset(&hooks, 0, sizeof(hooks));
    for (i = 0; i < n; ++i) {
        for (c = 0; c < 3; ++c) {
            state = state * 1664525U + 1013904223U;
            palette[i * 3 + c] = (unsigned char)(state >> 24);
            if (kind == 1) palette[i * 3 + c] = 42;
            if (kind == 2) palette[i * 3 + c] =
                (unsigned char)(((i >> (2 * c)) & 3) * 64);
            if (kind == 3) palette[i * 3 + c] = (unsigned char)i;
            if (kind == 4 && c == 2) palette[i * 3 + c] = 128;
            if (kind == 5) palette[i * 3 + c] =
                (unsigned char)((i & (1 << c)) ? 192 : 64);
        }
    }
    for (q = 0; q < 2048; ++q) {
        for (c = 0; c < 3; ++c) {
            state = state * 1664525U + 1013904223U;
            pixels[q * 3 + c] = (unsigned char)(state >> 24);
            /* Palette hits, exact pair midpoints, seed boundaries and
             * full-byte random queries sample different walk failures. */
            if (q < n) pixels[q * 3 + c] = palette[q * 3 + c];
            else if (q < 2 * n) pixels[q * 3 + c] = (unsigned char)
                (((palette[(q % n) * 3 + c] & 254)
                + (palette[((q + 1) % n) * 3 + c] & 254)) / 2);
            else if (q < 1024) pixels[q * 3 + c] = (unsigned char)
                (((state >> 24) & 240) + ((q % 3) - 1));
        }
    }
    request.policy = SIXEL_GPU_POLICY_AUTO;
    request.test_hooks = &hooks;
    request.dest = result;
    request.pixels = pixels;
    request.pixel_count = 2048;
    request.width = 64;
    request.height = 32;
    request.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    request.palette = palette;
    request.palette_size = (size_t)n * 3;
    request.palette_depth = 3;
    request.ncolors = n;
    request.lut_policy = SIXEL_LUT_POLICY_DELAUNAY;
    request.method_for_diffuse = SIXEL_DIFFUSE_NONE;
    request.method_for_scan = SIXEL_SCAN_RASTER;
    if (sixel_gpu_palette_apply(&request) != SIXEL_OK || hooks.kernel != 6) {
        return EXIT_FAILURE;
    }
    for (q = 0; q < 2048; ++q) {
        minimum = INT_MAX;
        expected = 0;
        for (i = 0; i < n; ++i) {
            distance = 0;
            for (c = 0; c < 3; ++c) {
                gap = (int)pixels[q * 3 + c] - palette[i * 3 + c];
                distance += gap * gap;
            }
            if (distance < minimum) {
                minimum = distance;
                expected = i;
            }
        }
        if (result[q] != expected) {
            fprintf(stderr, "geometry %d query %d: got %d expected %d\n",
                kind, q, (int)result[q], expected);
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
