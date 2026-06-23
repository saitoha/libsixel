/*
 * SPDX-License-Identifier: MIT
 *
 * Verify the k_undither row-band fast path matches scalar reconstruction.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "src/decoder.h"
#include "src/threading.h"

#define KUNDITHER_PARALLEL_WIDTH 512
#define KUNDITHER_PARALLEL_HEIGHT 512
#define KUNDITHER_PARALLEL_COLORS 80
#define KUNDITHER_PARALLEL_PIXELS \
    (KUNDITHER_PARALLEL_WIDTH * KUNDITHER_PARALLEL_HEIGHT)
#define KUNDITHER_PARALLEL_OUTPUT_SIZE \
    (KUNDITHER_PARALLEL_PIXELS * 3)
#define KUNDITHER_PARALLEL_RGBA_OUTPUT_SIZE \
    (KUNDITHER_PARALLEL_PIXELS * 4)

static void
kundither_parallel_init_palette(unsigned char *palette)
{
    int i;

    for (i = 0; i < KUNDITHER_PARALLEL_COLORS; ++i) {
        palette[i * 3 + 0] = (unsigned char)((i * 47 + 19) & 255);
        palette[i * 3 + 1] = (unsigned char)((i * 71 + 37) & 255);
        palette[i * 3 + 2] = (unsigned char)((i * 101 + 11) & 255);
    }
}

static void
kundither_parallel_init_indexed(unsigned char *indexed)
{
    int x;
    int y;
    int value;

    for (y = 0; y < KUNDITHER_PARALLEL_HEIGHT; ++y) {
        for (x = 0; x < KUNDITHER_PARALLEL_WIDTH; ++x) {
            value = x * 13 + y * 7 + ((x >> 3) ^ (y >> 2)) * 11;
            indexed[y * KUNDITHER_PARALLEL_WIDTH + x] =
                (unsigned char)(value % KUNDITHER_PARALLEL_COLORS);
        }
    }
}

static void
kundither_parallel_init_mask(unsigned char *paint_mask)
{
    int x;
    int y;

    for (y = 0; y < KUNDITHER_PARALLEL_HEIGHT; ++y) {
        for (x = 0; x < KUNDITHER_PARALLEL_WIDTH; ++x) {
            paint_mask[y * KUNDITHER_PARALLEL_WIDTH + x] =
                ((x * 3 + y * 5) % 11) == 0 ? 0x00U : 0xffU;
        }
    }
}

static int
kundither_parallel_decode(unsigned char const *indexed,
                          unsigned char const *palette,
                          int threads,
                          sixel_allocator_t *allocator,
                          unsigned char **output)
{
    SIXELSTATUS status;

    sixel_set_threads(threads);
    status = sixel_dequantize_k_undither(
        (unsigned char *)indexed,
        KUNDITHER_PARALLEL_WIDTH,
        KUNDITHER_PARALLEL_HEIGHT,
        (unsigned char *)palette,
        KUNDITHER_PARALLEL_COLORS,
        100,
        0,
        allocator,
        output);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "k_undither decode with %d threads failed: %04x\n",
                threads,
                status);
        return 0;
    }

    return *output != NULL;
}

static int
kundither_parallel_decode_rgba(unsigned char const *indexed,
                               unsigned char const *paint_mask,
                               unsigned char const *palette,
                               int threads,
                               sixel_allocator_t *allocator,
                               unsigned char **output)
{
    SIXELSTATUS status;

    sixel_set_threads(threads);
    status = sixel_dequantize_k_undither_rgba(
        (unsigned char *)indexed,
        paint_mask,
        KUNDITHER_PARALLEL_WIDTH,
        KUNDITHER_PARALLEL_HEIGHT,
        (unsigned char *)palette,
        KUNDITHER_PARALLEL_COLORS,
        100,
        0,
        allocator,
        output);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "k_undither RGBA decode with %d threads failed: %04x\n",
                threads,
                status);
        return 0;
    }

    return *output != NULL;
}

int
test_decoder_0013_decoder_kundither_parallel_matches_scalar(int argc,
                                                            char **argv)
{
    sixel_allocator_t *allocator;
    unsigned char *indexed;
    unsigned char *paint_mask;
    unsigned char palette[KUNDITHER_PARALLEL_COLORS * 3];
    unsigned char *serial;
    unsigned char *parallel;
    unsigned char *serial_rgba;
    unsigned char *parallel_rgba;
    size_t i;
    int success;

    (void)argc;
    (void)argv;

    allocator = NULL;
    indexed = NULL;
    paint_mask = NULL;
    serial = NULL;
    parallel = NULL;
    serial_rgba = NULL;
    parallel_rgba = NULL;
    success = 0;

    if (sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL) !=
        SIXEL_OK) {
        fprintf(stderr, "allocator setup failed\n");
        goto end;
    }

    indexed = (unsigned char *)sixel_allocator_malloc(
        allocator,
        KUNDITHER_PARALLEL_PIXELS);
    if (indexed == NULL) {
        fprintf(stderr, "indexed fixture allocation failed\n");
        goto end;
    }
    paint_mask = (unsigned char *)sixel_allocator_malloc(
        allocator,
        KUNDITHER_PARALLEL_PIXELS);
    if (paint_mask == NULL) {
        fprintf(stderr, "mask fixture allocation failed\n");
        goto end;
    }

    kundither_parallel_init_palette(palette);
    kundither_parallel_init_indexed(indexed);
    kundither_parallel_init_mask(paint_mask);

    if (!kundither_parallel_decode(indexed, palette, 1, allocator,
                                   &serial)) {
        goto end;
    }
    if (!kundither_parallel_decode(indexed, palette, 4, allocator,
                                   &parallel)) {
        goto end;
    }

    for (i = 0u; i < KUNDITHER_PARALLEL_OUTPUT_SIZE; ++i) {
        if (serial[i] != parallel[i]) {
            fprintf(stderr,
                    "parallel byte %lu is %u, expected scalar %u\n",
                    (unsigned long)i,
                    (unsigned int)parallel[i],
                    (unsigned int)serial[i]);
            goto end;
        }
    }
    if (!kundither_parallel_decode_rgba(indexed, paint_mask, palette, 1,
                                        allocator, &serial_rgba)) {
        goto end;
    }
    if (!kundither_parallel_decode_rgba(indexed, paint_mask, palette, 4,
                                        allocator, &parallel_rgba)) {
        goto end;
    }
    for (i = 0u; i < KUNDITHER_PARALLEL_RGBA_OUTPUT_SIZE; ++i) {
        if (serial_rgba[i] != parallel_rgba[i]) {
            fprintf(stderr,
                    "parallel RGBA byte %lu is %u,"
                    " expected scalar %u\n",
                    (unsigned long)i,
                    (unsigned int)parallel_rgba[i],
                    (unsigned int)serial_rgba[i]);
            goto end;
        }
    }

    success = 1;

end:
    if (allocator != NULL) {
        sixel_allocator_free(allocator, parallel_rgba);
        sixel_allocator_free(allocator, serial_rgba);
        sixel_allocator_free(allocator, parallel);
        sixel_allocator_free(allocator, serial);
        sixel_allocator_free(allocator, paint_mask);
        sixel_allocator_free(allocator, indexed);
        sixel_allocator_unref(allocator);
    }
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
