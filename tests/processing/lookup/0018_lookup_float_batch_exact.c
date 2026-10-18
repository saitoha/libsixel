/*
 * SPDX-License-Identifier: MIT
 * Policy: docs/encoder/palette-application/lookup-policy/README.md
 * Inspect the private kernel to verify exact keys and arithmetic directly.
 */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include "src/colorspace.h"
#define sixel_lookup_vptree_float32_create test_float_18_create
#define sixel_lookup_vptree_float32_unref test_float_18_unref
#define sixel_lookup_vptree_float32_configure test_float_18_configure
#define sixel_lookup_vptree_float32_map test_float_18_map
#define sixel_lookup_vptree_float32_workspace_size test_float_18_workspace_size
#define sixel_lookup_vptree_float32_map_pixels test_float_18_map_pixels
#include "src/lookup-vptree-float32.c"

int
test_lookup_0018_float_batch_exact(int argc, char **argv)
{
    sixel_allocator_t *allocator;
    sixel_lookup_vptree_float32_t *tree;
    sixel_float_workspace_t *scratch;
    float palette[257 * 3];
    float pixels[257][3];
    float sample[3];
    float weights[3] = { 1.0f, 0.5f, 2.0f };
    sixel_index_t actual[257], expected[257];
    uint32_t key[3];
    unsigned int slot, collision;
    int i, c, n, count, reverse, pass, rc;

    (void)argc;
    (void)argv;
    allocator = NULL;
    tree = NULL;
    scratch = NULL;
    rc = EXIT_FAILURE;
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL))
            || SIXEL_FAILED(sixel_lookup_vptree_float32_create(allocator,
                                                              &tree))) {
        goto cleanup;
    }
    scratch = calloc(1, sizeof(*scratch));
    if (scratch == NULL) {
        goto cleanup;
    }
    for (i = 0; i < 257; ++i) {
        for (c = 0; c < 3; ++c) {
            palette[i * 3 + c] = (float)((i * 37 + c * 19) % 256) / 255;
            pixels[i][c] = (float)((i * 17 + c * 31) % 256) / 255;
            if (i % 4 == 1) {
                pixels[i][c] = nextafterf(pixels[i][c], INFINITY);
            } else if (i % 4 == 2) {
                pixels[i][c] = pixels[i][c] * 4 - 1.5f;
            }
        }
        if (i % 4 == 3 && SIXEL_FAILED(sixel_color_convert_rgbf32(
                pixels[i], 1, SIXEL_COLORSPACE_GAMMA,
                SIXEL_COLORSPACE_CIELAB))) {
            goto cleanup;
        }
    }
    /* Duplicate colors and a representable midpoint force first-index ties. */
    for (c = 0; c < 3; ++c) {
        palette[c] = palette[6 + c] = -2.0f;
        palette[3 + c] = -4.0f;
        pixels[0][c] = -3.0f;
    }
    memcpy(pixels[1], pixels[0], sizeof(pixels[0]));
    memcpy(pixels[23], pixels[0], sizeof(pixels[0]));
    /* Find two distinct finite 96-bit keys in one actual memo slot. */
    sixel_vptree_float_memo_key(pixels[2], key);
    slot = sixel_vptree_float_memo_slot(key);
    for (collision = 1; collision < 1000000U; ++collision) {
        pixels[24][0] = (float)collision / 1024;
        sixel_vptree_float_memo_key(pixels[24], key);
        if (sixel_vptree_float_memo_slot(key) == slot) {
            break;
        }
    }
    if (collision == 1000000U) {
        goto cleanup;
    }
    pixels[25][0] = INFINITY;
    pixels[26][0] = NAN;
    pixels[27][0] = -INFINITY;
    /* Reprepare clears caller scratch; oversize palettes and NULL scratch
     * exercise the exact fallback used after optional allocation failure. */
    for (n = 16; n <= 257; n += n == 16 ? 240 : 1) {
        if (SIXEL_FAILED(sixel_lookup_vptree_float32_configure(tree,
                palette, n, 3, weights))) {
            goto cleanup;
        }
        memset(scratch, 0, sizeof(*scratch));
        for (i = 0; i < 257; ++i) {
            expected[i] = (sixel_index_t)
                sixel_lookup_vptree_float32_map(tree, pixels[i], -1);
        }
        if (expected[0] != 0) {
            goto cleanup;
        }
        for (reverse = 0; reverse < 2; ++reverse) {
            for (count = 0; count <= 257; ++count) {
                for (pass = 0; pass < 3; ++pass) {
                    if (pass == 1) {
                        memset(scratch, 0, sizeof(*scratch));
                    }
                    memset(actual, 0xff, sizeof(actual));
                    i = reverse && count ? count - 1 : 0;
                    sixel_lookup_vptree_float32_map_pixels(tree,
                        (unsigned char const *)pixels[i], (size_t)count,
                        reverse ? -12 : 12, actual + i, reverse ? -1 : 1,
                        pass == 0 ? NULL : scratch);
                    if (memcmp(actual, expected,
                               (size_t)count * sizeof(*actual)) != 0) {
                        fprintf(stderr, "n=%d count=%d reverse=%d pass=%d\n",
                                n, count, reverse, pass);
                        goto cleanup;
                    }
                }
            }
        }
        for (slot = 0; slot < SIXEL_VPTREE_FLOAT_MEMO_SIZE; ++slot) {
            if (scratch->memo[slot].index_plus_one != 0) {
                memcpy(sample, scratch->memo[slot].key, sizeof(key));
                if (n == 257 || !sixel_vptree_float_memo_key(sample, key)) {
                    goto cleanup;
                }
            }
        }
        if (n == 257 && sixel_lookup_vptree_float32_workspace_size(tree)) {
            goto cleanup;
        }
    }
    rc = EXIT_SUCCESS;
cleanup:
    free(scratch);
    sixel_lookup_vptree_float32_unref(tree);
    sixel_allocator_unref(allocator);
    return rc;
}
