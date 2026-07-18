/*
 * SPDX-License-Identifier: MIT
 *
 * Verify that selective_blur gates the fixed 3x3 blur by RGB distance.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "src/decoder.h"

static unsigned char const g_selective_blur_indexed[] = {
    0, 1, 2
};

static unsigned char const g_selective_blur_palette[] = {
    100, 0, 0,
    112, 0, 0,
    0, 0, 255
};

static unsigned char const g_selective_blur_threshold0_expected[] = {
    100, 0, 0,
    112, 0, 0,
    0, 0, 255
};

static unsigned char const g_selective_blur_threshold24_expected[] = {
    104, 0, 0,
    108, 0, 0,
    0, 0, 255
};

static int
selective_blur_compare(unsigned char const *actual,
                       unsigned char const *expected,
                       char const *label)
{
    size_t i;

    for (i = 0u; i < sizeof(g_selective_blur_threshold0_expected); ++i) {
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

int
test_decoder_0021_decoder_selective_blur_threshold(int argc, char **argv)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    unsigned char *threshold0;
    unsigned char *threshold24;
    int ok;

    (void)argc;
    (void)argv;

    allocator = NULL;
    threshold0 = NULL;
    threshold24 = NULL;
    ok = 0;

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_dequantize_selective_blur(
        (unsigned char *)g_selective_blur_indexed,
        3,
        1,
        (unsigned char *)g_selective_blur_palette,
        3,
        0,
        allocator,
        &threshold0);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "selective_blur threshold 0 failed: %04x\n", status);
        goto end;
    }

    status = sixel_dequantize_selective_blur(
        (unsigned char *)g_selective_blur_indexed,
        3,
        1,
        (unsigned char *)g_selective_blur_palette,
        3,
        24,
        allocator,
        &threshold24);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "selective_blur threshold 24 failed: %04x\n", status);
        goto end;
    }

    if (!selective_blur_compare(threshold0,
                                g_selective_blur_threshold0_expected,
                                "threshold 0")) {
        goto end;
    }
    if (!selective_blur_compare(threshold24,
                                g_selective_blur_threshold24_expected,
                                "threshold 24")) {
        goto end;
    }

    ok = 1;

end:
    if (allocator != NULL) {
        if (threshold0 != NULL) {
            sixel_allocator_free(allocator, threshold0);
        }
        if (threshold24 != NULL) {
            sixel_allocator_free(allocator, threshold24);
        }
        sixel_allocator_unref(allocator);
    }

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
