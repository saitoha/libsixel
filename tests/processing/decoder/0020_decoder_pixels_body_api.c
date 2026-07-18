/*
 * SPDX-License-Identifier: MIT
 *
 * Verify that sixel_decode_pixels_body() decodes the same DECSIXEL image as
 * the full-DCS memory API when the terminal parser supplies the DCS q params.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

static unsigned char const g_body_payload[] = "#1;2;100;0;0!2~";
static unsigned char const g_full_payload[] =
    "\033P0;5q#1;2;100;0;0!2~\033\\";
static int const g_dcs_params[] = { 0, 5 };

static int
body_api_results_match(sixel_decode_result_t const *left,
                       sixel_decode_result_t const *right)
{
    size_t bytes;

    bytes = 0U;

    if (left == NULL || right == NULL) {
        return 0;
    }
    if (left->width != right->width || left->height != right->height ||
            left->pixelformat != right->pixelformat ||
            left->stride != right->stride || left->flags != right->flags) {
        return 0;
    }
    if (left->width <= 0 || left->height <= 0 || left->stride <= 0 ||
            left->pixels == NULL || right->pixels == NULL) {
        return 0;
    }
    bytes = (size_t)left->stride * (size_t)left->height;
    return memcmp(left->pixels, right->pixels, bytes) == 0;
}

int
test_decoder_0020_decoder_pixels_body_api(int argc, char **argv)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_decode_options_t options;
    sixel_decode_result_t full;
    sixel_decode_result_t body;
    int ok;

    (void)argc;
    (void)argv;

    allocator = NULL;
    ok = 0;
    memset(&options, 0, sizeof(options));
    memset(&full, 0, sizeof(full));
    memset(&body, 0, sizeof(body));
    options.preferred_pixelformat = SIXEL_PIXELFORMAT_RGBA8888;

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_decode_pixels(g_full_payload,
                                 sizeof(g_full_payload) - 1U,
                                 &options,
                                 &full,
                                 allocator);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "full DCS decode failed: %04x\n", status);
        goto end;
    }

    status = sixel_decode_pixels_body(g_body_payload,
                                      sizeof(g_body_payload) - 1U,
                                      g_dcs_params,
                                      sizeof(g_dcs_params) /
                                          sizeof(g_dcs_params[0]),
                                      &options,
                                      &body,
                                      allocator);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "body decode failed: %04x\n", status);
        goto end;
    }
    if (!body_api_results_match(&full, &body)) {
        fprintf(stderr,
                "body decode mismatch full=%dx%d stride %d flags %u "
                "body=%dx%d stride %d flags %u\n",
                full.width,
                full.height,
                full.stride,
                full.flags,
                body.width,
                body.height,
                body.stride,
                body.flags);
        goto end;
    }

    ok = 1;

end:
    if (allocator != NULL) {
        if (full.pixels != NULL) {
            sixel_allocator_free(allocator, full.pixels);
        }
        if (body.pixels != NULL) {
            sixel_allocator_free(allocator, body.pixels);
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
