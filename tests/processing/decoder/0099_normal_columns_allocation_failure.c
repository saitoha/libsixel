/* SPDX-License-Identifier: MIT */
/* Fail every band allocation and growth step without leaking output state. */
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/sixel_decode_pixels.h"

static size_t columns_alloc_live;
static size_t columns_alloc_call;
static size_t columns_alloc_fail;

static void *columns_test_malloc(size_t size)
{
    void *p;

    if (++columns_alloc_call == columns_alloc_fail) {
        return NULL;
    }
    p = malloc(size);
    if (p != NULL) {
        ++columns_alloc_live;
    }
    return p;
}

static void *columns_test_calloc(size_t count, size_t size)
{
    void *p;

    p = columns_test_malloc(count * size);
    if (p != NULL) {
        memset(p, 0, count * size);
    }
    return p;
}

static void *columns_test_realloc(void *old, size_t size)
{
    void *p;
    int was_null;

    if (++columns_alloc_call == columns_alloc_fail) {
        return NULL;
    }
    was_null = old == NULL;
    p = realloc(old, size);
    if (p != NULL && was_null) {
        ++columns_alloc_live;
    }
    return p;
}

static void columns_test_free(void *p)
{
    if (p != NULL) {
        --columns_alloc_live;
    }
    free(p);
}

int test_decoder_0099(int argc, char **argv)
{
    unsigned char stream[] =
        "\033P0;1q\"1;1;2;2#1;2;100;0;0!5~-!7~\033\\";
    sixel_allocator_t *allocator;
    unsigned char *pixels;
    unsigned char *mask;
    unsigned char *palette;
    unsigned int flags;
    SIXELSTATUS status;
    size_t baseline;
    size_t fail;
    int width;
    int height;
    int ncolors;
    int ok;
    int completed;

    (void)argc;
    (void)argv;
    allocator = NULL;
    columns_alloc_live = columns_alloc_call = columns_alloc_fail = 0U;
    status = sixel_allocator_new(&allocator, columns_test_malloc,
        columns_test_calloc, columns_test_realloc, columns_test_free);
    if (SIXEL_FAILED(status)) { return EXIT_FAILURE; }
    baseline = columns_alloc_live;
    ok = 1;
    completed = 0;
    for (fail = 1U; fail <= 64U; ++fail) {
        pixels = mask = palette = NULL;
        columns_alloc_call = 0U;
        columns_alloc_fail = fail;
        status = sixel_decode_raw_with_options_mask(stream, sizeof(stream) - 1,
            0, &pixels, &mask, &width, &height, &palette, &ncolors,
            &flags, allocator);
        if (SIXEL_SUCCEEDED(status)) {
            completed = pixels != NULL && mask != NULL && palette != NULL &&
                width == 7 && height == 12 && pixels[11 * 7 + 6] == 1 &&
                mask[11 * 7 + 6] == 255;
        } else if (pixels != NULL || mask != NULL || palette != NULL) {
            ok = 0;
        }
        sixel_allocator_free(allocator, pixels);
        sixel_allocator_free(allocator, mask);
        sixel_allocator_free(allocator, palette);
        ok = ok && columns_alloc_live == baseline;
        if (completed || !ok) { break; }
    }
    columns_alloc_fail = 0U;
    sixel_allocator_unref(allocator);
    return ok && completed && columns_alloc_live == 0U ?
        EXIT_SUCCESS : EXIT_FAILURE;
}
