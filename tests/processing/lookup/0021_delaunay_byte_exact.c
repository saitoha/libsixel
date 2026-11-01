/*
 * SPDX-License-Identifier: MIT
 * Policy: docs/encoder/palette-application/lookup-policy/delaunay.md
 * Integer RGB oracle, independent of graph geometry and search order.
 */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#define sixel_lookup_delaunay_8bit_create test_dl21_create
#define sixel_lookup_delaunay_8bit_unref test_dl21_unref
#define sixel_lookup_delaunay_8bit_configure test_dl21_configure
#define sixel_lookup_delaunay_8bit_map test_dl21_map
#define sixel_lookup_delaunay_8bit_map_pixels test_dl21_map_pixels
#include "src/lookup-delaunay-8bit.c"

/* Bound routine checks without confining them to a low-red RGB prefix.
 * Keep exhaustive enumeration available for deliberate full-domain runs.
 * The grid includes channel endpoints, palette neighborhoods cover exact
 * hits and the index-255 sentinel, and midpoint neighbors cross bisectors.
 * Fixed random keys exercise the rest of the cube and memo collisions. */
static void
test_dl21_sample(unsigned int q, unsigned int *state,
                 unsigned char const *palette, int n, unsigned char *pixel)
{
    unsigned int local;
    int i, j, c, v, offset;

    if (q < 4913U) {
        for (c = 0; c < 3; ++c) {
            v = (int)(q % 17U) * 16;
            pixel[c] = (unsigned char)(v < 256 ? v : 255);
            q /= 17U;
        }
    } else if (q < 9009U) {
        *state = *state * 1664525U + 1013904223U;
        for (c = 0; c < 3; ++c) {
            pixel[c] = (unsigned char)(*state >> (8 * c));
        }
    } else if (q < 9009U + 27U * (unsigned int)n) {
        local = q - 9009U;
        i = (int)(local / 27U);
        local %= 27U;
        for (c = 0; c < 3; ++c) {
            v = palette[i * 3 + c] + (int)(local % 3U) - 1;
            pixel[c] = (unsigned char)(v < 0 ? 0 : v > 255 ? 255 : v);
            local /= 3U;
        }
    } else {
        local = q - 9009U - 27U * (unsigned int)n;
        i = (int)(local / 21U);
        j = (int)(local / 7U % 3U);
        j = (i + (j == 0 ? 1 : j == 1 ? n / 2 : n - 1)) % n;
        offset = (int)(local % 7U);
        for (c = 0; c < 3; ++c) {
            v = (palette[i * 3 + c] + palette[j * 3 + c]) / 2;
            if (offset != 0 && (offset - 1) / 2 == c) {
                v += offset % 2 == 0 ? 1 : -1;
            }
            pixel[c] = (unsigned char)(v < 0 ? 0 : v > 255 ? 255 : v);
        }
    }
}

int
test_lookup_0021_delaunay_byte_exact(int argc, char **argv)
{
    sixel_allocator_t *allocator;
    sixel_lookup_delaunay_8bit_t *graph;
    unsigned char palette[300 * 3], pixel[3];
    char const *method;
    int (*squares)[256][300];
    unsigned int state, q, limit;
    int n, kind, i, c, v, d, best, expected, actual, center, rc, sampled;

    allocator = NULL;
    graph = NULL;
    rc = EXIT_FAILURE;
    n = argc > 1 ? atoi(argv[1]) : 256;
    kind = argc > 2 ? atoi(argv[2]) : 0;
    method = argc > 4 ? argv[4] : "map";
    sampled = argc <= 3 || strcmp(argv[3], "sample") == 0;
    limit = sampled ? 9009U + 48U * (unsigned int)n :
        (unsigned int)strtoul(argv[3], NULL, 0);
    if (n < 1 || n > 300 || limit == 0 || limit > (1U << 24)) {
        return EXIT_FAILURE;
    }
    if (strcmp(method, "map") != 0 && strcmp(method, "scalar") != 0 &&
            strcmp(method, "simd") != 0 && strcmp(method, "stale") != 0) {
        return EXIT_FAILURE;
    }
#if !SIXEL_DL8_SIMD
    if (strcmp(method, "simd") == 0) {
        return 77;
    }
#endif
    /* The 900 KiB oracle table exceeds the default WebAssembly stack.
     * Keep it on the heap in both sampled and exhaustive modes. */
    squares = malloc(3 * sizeof(*squares));
    if (squares == NULL) {
        return EXIT_FAILURE;
    }
    state = 1841U;
    for (i = 0; i < n; ++i) {
        for (c = 0; c < 3; ++c) {
            state = state * 1664525U + 1013904223U;
            palette[i * 3 + c] = (unsigned char)(state >> 24);
            if (kind == 1 && i >= 40) {
                palette[i * 3 + c] = palette[(i % 40) * 3 + c];
            } else if (kind == 2) {
                palette[i * 3 + c] = (unsigned char)
                    (((i >> (2 * c)) & 3) * 64);
            }
        }
    }
    if (n == 256 && kind == 0) {
        memset(palette + 255 * 3, 255, 3);
    }
    for (c = 0; c < 3; ++c) {
        for (v = 0; v < 256; ++v) {
            for (i = 0; i < n; ++i) {
                d = v - palette[i * 3 + c];
                squares[c][v][i] = d * d;
            }
        }
    }
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL))
            || SIXEL_FAILED(test_dl21_create(allocator, &graph))
            || SIXEL_FAILED(test_dl21_configure(graph, palette, n, 3))) {
        goto cleanup;
    }
    fprintf(stderr, "delaunay byte n=%d kind=%d queries=%u method=%s\n",
            n, kind, limit, method);
    for (q = 0; q < limit; ++q) {
        if (sampled) {
            test_dl21_sample(q, &state, palette, n, pixel);
        } else {
            pixel[0] = (unsigned char)(q >> 16);
            pixel[1] = (unsigned char)(q >> 8);
            pixel[2] = (unsigned char)q;
        }
        best = INT_MAX;
        expected = 0;
        /* Precomputed per-channel integer squares only reduce test cost;
         * every palette entry still participates in every oracle query. */
        for (i = 0; i < n; ++i) {
            d = squares[0][pixel[0]][i] + squares[1][pixel[1]][i]
                + squares[2][pixel[2]][i];
            if (d < best) {
                best = d;
                expected = i;
            }
        }
        center = graph->pal2u[(q * 37U) % (unsigned int)n];
        /* Each TAP file selects one consumer. In particular, a stale-hint
         * lookup must not inherit memo entries from an earlier map check. */
        if (strcmp(method, "scalar") == 0) {
            actual = sixel_lookup_delaunay_8bit_walk_scalar(graph, pixel,
                                                          center);
#if SIXEL_DL8_SIMD
        } else if (strcmp(method, "simd") == 0) {
            actual = sixel_lookup_delaunay_8bit_walk_simd(graph, pixel,
                                                        center);
#endif
        } else {
            actual = test_dl21_map(graph, pixel,
                strcmp(method, "stale") == 0 ? n + 3 : -1);
        }
        if (actual != expected) {
            fprintf(stderr, "RGB=%02x%02x%02x expected=%d actual=%d "
                    "center=%d\n", pixel[0], pixel[1], pixel[2],
                    expected, actual, center);
            goto cleanup;
        }
    }
    rc = EXIT_SUCCESS;
cleanup:
    free(squares);
    test_dl21_unref(graph);
    sixel_allocator_unref(allocator);
    return rc;
}
