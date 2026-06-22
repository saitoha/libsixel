/*
 * SPDX-License-Identifier: MIT
 *
 * Shared fixture for k_undither pixel-exact decoder tests.
 */

#ifndef LIBSIXEL_TESTS_PROCESSING_DECODER_KUNDITHER_TEST_COMMON_H
#define LIBSIXEL_TESTS_PROCESSING_DECODER_KUNDITHER_TEST_COMMON_H

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "src/decoder.h"

#define KUNDITHER_TEST_WIDTH 6
#define KUNDITHER_TEST_HEIGHT 4
#define KUNDITHER_TEST_COLORS 32
#define KUNDITHER_TEST_OUTPUT_SIZE \
    (KUNDITHER_TEST_WIDTH * KUNDITHER_TEST_HEIGHT * 3)

static unsigned char const g_kundither_test_indexed[] = {
    0, 1, 2, 3, 4, 5,
    6, 7, 8, 9, 10, 11,
    12, 13, 14, 15, 16, 17,
    18, 19, 20, 21, 22, 23
};

static void
kundither_test_init_palette(unsigned char *palette)
{
    int i;

    for (i = 0; i < KUNDITHER_TEST_COLORS; ++i) {
        palette[i * 3 + 0] = (unsigned char)((i * 37 + 11) & 255);
        palette[i * 3 + 1] = (unsigned char)((i * 67 + 23) & 255);
        palette[i * 3 + 2] = (unsigned char)((i * 97 + 5) & 255);
    }
}

static int
kundither_test_compare(unsigned char const *actual,
                       unsigned char const *expected,
                       size_t length,
                       char const *label)
{
    size_t i;

    for (i = 0u; i < length; ++i) {
        if (actual[i] != expected[i]) {
            fprintf(stderr,
                    "%s byte %lu is %u, expected %u\n",
                    label,
                    (unsigned long)i,
                    (unsigned int)actual[i],
                    (unsigned int)expected[i]);
            return 0;
        }
    }

    return 1;
}

static int
kundither_test_run(char const *label,
                   int similarity_bias,
                   int edge_strength,
                   unsigned char const *expected)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    unsigned char palette[KUNDITHER_TEST_COLORS * 3];
    unsigned char *output;
    int ok;

    allocator = NULL;
    output = NULL;
    ok = 0;
    kundither_test_init_palette(palette);

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_dequantize_k_undither(
        (unsigned char *)g_kundither_test_indexed,
        KUNDITHER_TEST_WIDTH,
        KUNDITHER_TEST_HEIGHT,
        palette,
        KUNDITHER_TEST_COLORS,
        similarity_bias,
        edge_strength,
        allocator,
        &output);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s failed with status %04x\n", label, status);
        goto end;
    }

    ok = kundither_test_compare(
        output,
        expected,
        KUNDITHER_TEST_OUTPUT_SIZE,
        label);

end:
    if (allocator != NULL) {
        if (output != NULL) {
            sixel_allocator_free(allocator, output);
        }
        sixel_allocator_unref(allocator);
    }

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

#endif /* LIBSIXEL_TESTS_PROCESSING_DECODER_KUNDITHER_TEST_COMMON_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
