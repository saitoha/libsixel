/*
 * SPDX-License-Identifier: MIT
 *
 * Verify fast4 k_undither parallel output matches scalar reconstruction.
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

#define KUNDITHER_FAST4_WIDTH 512
#define KUNDITHER_FAST4_HEIGHT 512
#define KUNDITHER_FAST4_COLORS 80
#define KUNDITHER_FAST4_PIXELS \
    (KUNDITHER_FAST4_WIDTH * KUNDITHER_FAST4_HEIGHT)
#define KUNDITHER_FAST4_OUTPUT_SIZE \
    (KUNDITHER_FAST4_PIXELS * 3)

static void
kundither_fast4_init_palette(unsigned char *palette)
{
    int i;

    for (i = 0; i < KUNDITHER_FAST4_COLORS; ++i) {
        palette[i * 3 + 0] = (unsigned char)((i * 43 + 17) & 255);
        palette[i * 3 + 1] = (unsigned char)((i * 73 + 29) & 255);
        palette[i * 3 + 2] = (unsigned char)((i * 109 + 13) & 255);
    }
}

static void
kundither_fast4_init_indexed(unsigned char *indexed)
{
    int x;
    int y;
    int value;

    for (y = 0; y < KUNDITHER_FAST4_HEIGHT; ++y) {
        for (x = 0; x < KUNDITHER_FAST4_WIDTH; ++x) {
            value = x * 17 + y * 5 + ((x >> 2) ^ (y >> 4)) * 19;
            indexed[y * KUNDITHER_FAST4_WIDTH + x] =
                (unsigned char)(value % KUNDITHER_FAST4_COLORS);
        }
    }
}

static int
kundither_fast4_decode(unsigned char const *indexed,
                       unsigned char const *palette,
                       int threads,
                       sixel_allocator_t *allocator,
                       unsigned char **output)
{
    SIXELSTATUS status;

    sixel_set_threads(threads);
    status = sixel_dequantize_k_undither_fast4(
        (unsigned char *)indexed,
        KUNDITHER_FAST4_WIDTH,
        KUNDITHER_FAST4_HEIGHT,
        (unsigned char *)palette,
        KUNDITHER_FAST4_COLORS,
        100,
        allocator,
        output);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "fast4 k_undither decode with %d threads failed: %04x\n",
                threads,
                status);
        return 0;
    }

    return *output != NULL;
}

int
test_decoder_0014_decoder_kundither_fast4_parallel_matches_scalar(
    int argc,
    char **argv)
{
    sixel_allocator_t *allocator;
    unsigned char *indexed;
    unsigned char palette[KUNDITHER_FAST4_COLORS * 3];
    unsigned char *serial;
    unsigned char *parallel;
    size_t i;
    int success;

    (void)argc;
    (void)argv;

    allocator = NULL;
    indexed = NULL;
    serial = NULL;
    parallel = NULL;
    success = 0;

    if (sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL) !=
            SIXEL_OK) {
        fprintf(stderr, "allocator setup failed\n");
        goto end;
    }

    indexed = (unsigned char *)sixel_allocator_malloc(
        allocator,
        KUNDITHER_FAST4_PIXELS);
    if (indexed == NULL) {
        fprintf(stderr, "indexed fixture allocation failed\n");
        goto end;
    }

    kundither_fast4_init_palette(palette);
    kundither_fast4_init_indexed(indexed);

    if (!kundither_fast4_decode(indexed, palette, 1, allocator, &serial)) {
        goto end;
    }
    if (!kundither_fast4_decode(indexed, palette, 4, allocator, &parallel)) {
        goto end;
    }

    for (i = 0u; i < KUNDITHER_FAST4_OUTPUT_SIZE; ++i) {
        if (serial[i] != parallel[i]) {
            fprintf(stderr,
                    "parallel byte %lu is %u, expected scalar %u\n",
                    (unsigned long)i,
                    (unsigned int)parallel[i],
                    (unsigned int)serial[i]);
            goto end;
        }
    }

    success = 1;

end:
    if (allocator != NULL) {
        sixel_allocator_free(allocator, parallel);
        sixel_allocator_free(allocator, serial);
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
