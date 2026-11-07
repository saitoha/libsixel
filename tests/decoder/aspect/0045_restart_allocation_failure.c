/* SPDX-License-Identifier: MIT */
/* Sweep every caller-allocator failure across both attempts, including
 * masks, columns, OR planes, direct RGBA and speculative fused outputs.
 * Optional allocation failures may succeed: do not stop the sweep early. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/decoder-parallel.h"
#include "src/sixel_decode_pixels.h"

static size_t aspect_restart_alloc_live;
static size_t aspect_restart_alloc_call;
static size_t aspect_restart_alloc_fail;

static void *aspect_restart_test_malloc(size_t size)
{
    void *p;

    if (++aspect_restart_alloc_call == aspect_restart_alloc_fail) {
        return NULL;
    }
    p = malloc(size);
    if (p != NULL) {
        ++aspect_restart_alloc_live;
    }
    return p;
}

static void *aspect_restart_test_calloc(size_t count, size_t size)
{
    void *p;

    p = aspect_restart_test_malloc(count * size);
    if (p != NULL) {
        memset(p, 0, count * size);
    }
    return p;
}

static void *aspect_restart_test_realloc(void *old, size_t size)
{
    void *p;
    int was_null;

    if (++aspect_restart_alloc_call == aspect_restart_alloc_fail) {
        return NULL;
    }
    was_null = old == NULL;
    p = realloc(old, size);
    if (p != NULL && was_null) {
        ++aspect_restart_alloc_live;
    }
    return p;
}

static void aspect_restart_test_free(void *p)
{
    if (p != NULL) {
        --aspect_restart_alloc_live;
    }
    free(p);
}


int
test_decoder_aspect_0045(int argc, char **argv)
{
    static char const *const streams[] = {
        "\033Pq\"1;1;16;18#1;2;100;0;0!16~-\"2;1;16;18!16~\033\\",
        "\033P0;5q\"1;1;16;18#257!16~-\"2;1;16;18!16~\033\\",
        "\033Pq#1;2;100;0;0!16~-\"2;1;16;18!16~\033\\"
    };
    sixel_allocator_t *allocator;
    unsigned char *pixels;
    unsigned char *mask;
    unsigned char *palette;
    unsigned char *stream;
    unsigned int flags;
    SIXELSTATUS status;
    size_t baseline;
    size_t calls;
    size_t fail;
    int width;
    int height;
    int ncolors;
    int mode;
    int specimen;
    int budget;
    int ok;

    (void)argc;
    (void)argv;
    allocator = NULL;
    aspect_restart_alloc_live = 0U;
    aspect_restart_alloc_call = 0U;
    aspect_restart_alloc_fail = 0U;
    status = sixel_allocator_new(&allocator, aspect_restart_test_malloc,
        aspect_restart_test_calloc, aspect_restart_test_realloc,
        aspect_restart_test_free);
    if (status != SIXEL_OK) {
        return EXIT_FAILURE;
    }
    baseline = aspect_restart_alloc_live;
    ok = 1;
    for (budget = 0; budget < 2 && ok; ++budget) {
        status = sixel_decoder_parallel_override_threads(budget ? "4" : "1");
        ok = status == SIXEL_OK;
        for (specimen = 0; specimen < 3 && ok; ++specimen) {
            stream = (unsigned char *)(void *)streams[specimen];
            for (mode = 0; mode < 4 && ok; ++mode) {
                /* High-register OR reconstruction is outside fast4's byte
                 * palette contract; its raw and direct outputs still run. */
                if (specimen == 1 && mode >= 2) {
                    continue;
                }
                calls = 0U;
                for (fail = 0U; fail <= calls && ok; ++fail) {
                    pixels = mask = palette = NULL;
                    aspect_restart_alloc_call = 0U;
                    aspect_restart_alloc_fail = fail;
                    if (mode == 0) {
                        status = sixel_decode_raw_with_options_mask(stream,
                            (int)strlen((char *)stream), 0U, &pixels, &mask,
                            &width, &height, &palette, &ncolors, &flags,
                            allocator);
                    } else if (mode == 1) {
                        status = sixel_decode_direct_with_options(stream,
                            (int)strlen((char *)stream), 0U, &pixels,
                            &width, &height, &palette, &ncolors, &flags,
                            allocator);
                    } else {
                        status = sixel_decode_kundither_fast4_with_options(
                            stream, (int)strlen((char *)stream), mode == 3,
                            0, 0U, &flags, &pixels, &width, &height, allocator);
                    }
                    if (fail == 0U) {
                        calls = aspect_restart_alloc_call;
                        ok = status == SIXEL_OK && calls > 1U;
                    }
                    if (status == SIXEL_OK) {
                        ok = ok && pixels != NULL && width == 16 &&
                            height == 18 && flags == 0U;
                        if (mode == 0) {
                            ok = ok && mask != NULL && palette != NULL &&
                                mask[0] == 255 && mask[16 * 18 - 1] == 255;
                        }
                    } else {
                        ok = ok && SIXEL_FAILED(status) && status >= 0 &&
                            pixels == NULL && mask == NULL && palette == NULL;
                    }
                    sixel_allocator_free(allocator, pixels);
                    sixel_allocator_free(allocator, mask);
                    sixel_allocator_free(allocator, palette);
                    ok = ok && aspect_restart_alloc_live == baseline;
                    if (!ok) {
                        fprintf(stderr, "restart OOM: budget=%d stream=%d "
                            "mode=%d fail=%zu/%zu status=%x live=%zu/%zu\n",
                            budget, specimen, mode, fail, calls, status,
                            aspect_restart_alloc_live, baseline);
                    }
                }
            }
        }
    }
    aspect_restart_alloc_fail = 0U;
    (void)sixel_decoder_parallel_override_threads("1");
    sixel_allocator_unref(allocator);
    return ok && aspect_restart_alloc_live == 0U ? EXIT_SUCCESS : EXIT_FAILURE;
}
