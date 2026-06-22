/*
 * SPDX-License-Identifier: MIT
 *
 * Verify that sixel_decode_pixels() returns the requested packed byte order
 * and marks fully opaque output so callers can use RGBX/XRGB fast paths.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

typedef struct decoder_pixels_format_case {
    int pixelformat;
    char const *name;
    unsigned char expected[4];
} decoder_pixels_format_case_t;

static unsigned char const g_output_formats_payload[] =
    "\033Pq\"1;1;2;1#1;2;100;0;0#1@@\033\\";

static decoder_pixels_format_case_t const g_output_format_cases[] = {
    { SIXEL_PIXELFORMAT_XRGB8888, "XRGB8888", { 0xffU, 0xffU, 0x00U, 0x00U } },
    { SIXEL_PIXELFORMAT_RGBX8888, "RGBX8888", { 0xffU, 0x00U, 0x00U, 0xffU } },
    { SIXEL_PIXELFORMAT_XBGR8888, "XBGR8888", { 0xffU, 0x00U, 0x00U, 0xffU } },
    { SIXEL_PIXELFORMAT_BGRX8888, "BGRX8888", { 0x00U, 0x00U, 0xffU, 0xffU } }
};

int
test_decoder_0008_decoder_pixels_output_formats(int argc, char **argv)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_decode_options_t options;
    sixel_decode_result_t result;
    size_t i;
    size_t cases;
    int ok;

    (void)argc;
    (void)argv;

    allocator = NULL;
    i = 0U;
    cases = sizeof(g_output_format_cases) / sizeof(g_output_format_cases[0]);
    ok = 0;
    memset(&options, 0, sizeof(options));
    memset(&result, 0, sizeof(result));

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    for (i = 0U; i < cases; ++i) {
        options.flags = 0U;
        options.preferred_pixelformat = g_output_format_cases[i].pixelformat;
        options.bgcolor[0] = 0U;
        options.bgcolor[1] = 0U;
        options.bgcolor[2] = 0U;
        memset(&result, 0, sizeof(result));

        status = sixel_decode_pixels(
            g_output_formats_payload,
            sizeof(g_output_formats_payload) - 1U,
            &options,
            &result,
            allocator);
        if (SIXEL_FAILED(status)) {
            fprintf(stderr, "%s decode failed: %04x\n",
                    g_output_format_cases[i].name,
                    status);
            goto end;
        }
        if (result.width != 2 || result.height != 1 || result.stride != 8) {
            fprintf(stderr, "%s dimensions are %dx%d stride %d\n",
                    g_output_format_cases[i].name,
                    result.width,
                    result.height,
                    result.stride);
            goto end;
        }
        if (result.pixelformat != g_output_format_cases[i].pixelformat) {
            fprintf(stderr, "%s returned pixelformat %d\n",
                    g_output_format_cases[i].name,
                    result.pixelformat);
            goto end;
        }
        if (result.pixels == NULL ||
                memcmp(result.pixels,
                       g_output_format_cases[i].expected,
                       4U) != 0) {
            fprintf(stderr, "%s first pixel is not in requested byte order\n",
                    g_output_format_cases[i].name);
            goto end;
        }
        if ((result.flags & SIXEL_DECODE_PIXELS_RESULT_ALPHA_OPAQUE) == 0U) {
            fprintf(stderr, "%s did not report opaque alpha\n",
                    g_output_format_cases[i].name);
            goto end;
        }

        sixel_allocator_free(allocator, result.pixels);
        result.pixels = NULL;
    }

    ok = 1;

end:
    if (allocator != NULL) {
        if (result.pixels != NULL) {
            sixel_allocator_free(allocator, result.pixels);
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
