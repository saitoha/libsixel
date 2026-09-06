/*
 * SPDX-License-Identifier: MIT
 *
 * Verify that a partial direct paint worker launch fails before any worker
 * can modify the serial fallback image.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "src/compat_stub.h"
#include "src/decoder-image.h"
#include "src/decoder-parallel.h"

#define PAINT_CREATE_WIDTH 8
#define PAINT_CREATE_HEIGHT 12
#define PAINT_CREATE_PIXEL_SENTINEL 0xa5
#define PAINT_CREATE_MASK_SENTINEL 0x5a

static unsigned char g_paint_create_payload[] = "#1!8~-#1!8~";

static int
paint_create_bytes_equal(unsigned char const *data,
                         size_t bytes,
                         unsigned char expected)
{
    size_t i;

    if (data == NULL) {
        return 0;
    }
    for (i = 0u; i < bytes; ++i) {
        if (data[i] != expected) {
            return 0;
        }
    }
    return 1;
}

int
test_decoder_0025_decoder_paint_create_failure_clean(int argc, char **argv)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    image_buffer_t *image;
    size_t pixels;
    int ok;

    (void)argc;
    (void)argv;

    allocator = NULL;
    image = NULL;
    pixels = (size_t)PAINT_CREATE_WIDTH * (size_t)PAINT_CREATE_HEIGHT;
    ok = 0;

    status = sixel_decoder_parallel_override_threads("2");
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (sixel_compat_setenv(
            "_SIXEL_TEST_DECODER_PAINT_THREAD_CREATE_FAILURE", "1") != 0) {
        goto end;
    }
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        allocator = NULL;
        goto end;
    }

    image = (image_buffer_t *)malloc(sizeof(*image));
    if (image == NULL) {
        goto end;
    }
    memset(image, 0, sizeof(*image));
    status = image_buffer_init(image,
                               PAINT_CREATE_WIDTH,
                               PAINT_CREATE_HEIGHT,
                               0,
                               1,
                               1,
                               allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    memset(image->pixels.p, PAINT_CREATE_PIXEL_SENTINEL, pixels);
    memset(image->paint_mask, PAINT_CREATE_MASK_SENTINEL, pixels);
    status = sixel_decoder_parallel_request_start(
        0,
        0,
        g_paint_create_payload,
        (int)(sizeof(g_paint_create_payload) - 1U),
        g_paint_create_payload,
        image,
        1,
        image->palette,
        NULL,
        0U,
        NULL,
        NULL);
    if (status != SIXEL_FALSE) {
        fprintf(stderr, "paint create failure returned %04x\n", status);
        goto end;
    }
    if (!paint_create_bytes_equal(image->pixels.p,
                                  pixels,
                                  PAINT_CREATE_PIXEL_SENTINEL)) {
        fprintf(stderr, "paint create failure dirtied indexed pixels\n");
        goto end;
    }
    if (!paint_create_bytes_equal(image->paint_mask,
                                  pixels,
                                  PAINT_CREATE_MASK_SENTINEL)) {
        fprintf(stderr, "paint create failure dirtied paint mask\n");
        goto end;
    }

    ok = 1;

end:
    (void)sixel_compat_setenv(
        "_SIXEL_TEST_DECODER_PAINT_THREAD_CREATE_FAILURE", "");
    (void)sixel_decoder_parallel_override_threads("1");
    if (image != NULL) {
        if (allocator != NULL && image->paint_mask != NULL) {
            sixel_allocator_free(allocator, image->paint_mask);
        }
        if (allocator != NULL && image->pixels.p != NULL) {
            sixel_allocator_free(allocator, image->pixels.p);
        }
        free(image);
    }
    if (allocator != NULL) {
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
