/*
 * SPDX-License-Identifier: MIT
 * Policy: docs/encoder/palette-application/lookup-policy/delaunay.md
 * Cold batches match an independent integer scan across cache collisions,
 * duplicate representatives, tails, reverse strides and sentinel entries.
 */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#define sixel_lookup_delaunay_8bit_create test_dl34_create
#define sixel_lookup_delaunay_8bit_unref test_dl34_unref
#define sixel_lookup_delaunay_8bit_configure test_dl34_configure
#define sixel_lookup_delaunay_8bit_map test_dl34_map
#define sixel_lookup_delaunay_8bit_map_pixels test_dl34_map_pixels
#include "src/lookup-delaunay-8bit.c"

int
test_lookup_0034_delaunay_byte_bulk(int argc, char **argv)
{
    sixel_allocator_t *allocator;
    sixel_lookup_delaunay_8bit_t *graph;
    unsigned char palette[257 * 3], pixels[65][4];
    sixel_index_t actual[132], expected[65];
    int sizes[] = { 1, 2, 3, 7, 40, 64, 256, 257 };
    int counts[] = { 0, 1, 7, 8, 9, 15, 16, 17, 23, 24, 25, 31, 32, 33, 65 };
    unsigned int state, key, slot, collision;
    int i, j, c, n, size, count, length, reverse, pass, start, stride;
    int best, distance, delta, minimum, rc;
#if SIXEL_DL8_SIMD
    unsigned int keys[8];
    int indexes[8];
#endif

    (void)argc;
    (void)argv;
    allocator = NULL;
    graph = NULL;
    rc = EXIT_FAILURE;
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL))
            || SIXEL_FAILED(test_dl34_create(allocator, &graph))) {
        goto cleanup;
    }
    state = 27;
    for (i = 0; i < 257; ++i) {
        for (c = 0; c < 3; ++c) {
            state = state * 1664525U + 1013904223U;
            palette[i * 3 + c] = (unsigned char)(state >> 24);
            if (i > 0 && i % 5 == 0) {
                palette[i * 3 + c] = palette[(i - 1) * 3 + c];
            }
        }
    }
    memset(palette, 0, 9);
    palette[3] = 2;
    memset(palette + 255 * 3, 255, 3);
    for (i = 0; i < 65; ++i) {
        for (c = 0; c < 3; ++c) {
            state = state * 1664525U + 1013904223U;
            pixels[i][c] = (unsigned char)(state >> 24);
        }
        pixels[i][3] = 0x55;
        if (i > 0 && i % 3 == 1) {
            memcpy(pixels[i], pixels[i - 1], 3);
        }
    }
    pixels[0][0] = 1;
    pixels[0][1] = pixels[0][2] = 0;
    memset(pixels[7], 255, 3);
    memset(pixels[8], 255, 3);
    key = ((unsigned int)pixels[23][0] << 16)
        | ((unsigned int)pixels[23][1] << 8) | pixels[23][2];
    slot = (key * 2654435761U) >> (32 - SIXEL_DELAUNAY_BYTE_MEMO_BITS);
    for (collision = 0; collision < (1U << 24); ++collision) {
        if (collision != key && (collision * 2654435761U)
                >> (32 - SIXEL_DELAUNAY_BYTE_MEMO_BITS) == slot) {
            break;
        }
    }
    if (collision == (1U << 24)) {
        goto cleanup;
    }
    pixels[24][0] = (unsigned char)(collision >> 16);
    pixels[24][1] = (unsigned char)(collision >> 8);
    pixels[24][2] = (unsigned char)collision;
    fprintf(stderr, "delaunay byte cold8 SIMD=%d\n", SIXEL_DL8_SIMD);
    for (size = 0; size < 8; ++size) {
        n = sizes[size];
        if (SIXEL_FAILED(test_dl34_configure(graph, palette, n, 3))) {
            goto cleanup;
        }
        for (i = 0; i < 65; ++i) {
            best = 0;
            minimum = INT_MAX;
            for (j = 0; j < n; ++j) {
                distance = 0;
                for (c = 0; c < 3; ++c) {
                    delta = (int)pixels[i][c] - palette[j * 3 + c];
                    distance += delta * delta;
                }
                if (distance < minimum) {
                    minimum = distance;
                    best = j;
                }
            }
            expected[i] = (sixel_index_t)best;
        }
#if SIXEL_DL8_SIMD
        /* Call the actual eight-query kernel even on a one-color palette.
         * IDs after duplicate collapse must remain original indices. */
        for (i = 0; i < 8; ++i) {
            keys[i] = ((unsigned int)pixels[i][0] << 16)
                | ((unsigned int)pixels[i][1] << 8) | pixels[i][2];
        }
        sixel_lookup_delaunay_8bit_map_cold8(graph, keys, indexes);
        for (i = 0; i < 8; ++i) {
            if ((sixel_index_t)indexes[i] != expected[i]) {
                goto cleanup;
            }
        }
#endif
        for (reverse = 0; reverse < 2; ++reverse) {
            for (length = 0; length < 15; ++length) {
                count = counts[length];
                if (graph->memo != NULL) {
                    for (i = 0; i < (1 << SIXEL_DELAUNAY_BYTE_MEMO_BITS);
                            ++i) {
                        sixel_atomic_store_relaxed_i32(&graph->memo[i], -1);
                    }
                }
                start = reverse && count ? count - 1 : 0;
                stride = reverse ? -1 : 1;
                for (pass = 0; pass < 2; ++pass) {
                    memset(actual, 0xa5, sizeof(actual));
                    test_dl34_map_pixels(graph, pixels[start], (size_t)count,
                        stride * 4, actual + 1 + start * 2, stride * 2);
                    for (i = 0; i < 65; ++i) {
                        if (actual[1 + i * 2]
                                != (i < count ? expected[i] : 0xa5)
                                || actual[i * 2] != 0xa5) {
                            fprintf(stderr, "bulk n=%d count=%d reverse=%d\n",
                                    n, count, reverse);
                            goto cleanup;
                        }
                    }
                    if (actual[130] != 0xa5 || actual[131] != 0xa5) {
                        goto cleanup;
                    }
                }
            }
        }
    }
    rc = EXIT_SUCCESS;
cleanup:
    test_dl34_unref(graph);
    sixel_allocator_unref(allocator);
    return rc;
}
