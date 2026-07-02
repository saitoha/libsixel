/*
 * SPDX-License-Identifier: MIT
 *
 * Verify direct parallel preflight rejects coordinate overflow before paint
 * workers can dirty the image.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "src/decoder-image.h"
#include "src/decoder-parallel.h"

#define DIRECT_REPEAT_WIDTH 8
#define DIRECT_REPEAT_HEIGHT 12
#define DIRECT_REPEAT_PIXEL_SENTINEL 0xa5
#define DIRECT_REPEAT_MASK_SENTINEL 0x5a
#define DIRECT_REPEAT_COUNT 33000

static int
direct_repeat_make_payload(unsigned char **payload, int *payload_len)
{
    unsigned char *data;
    size_t length;
    size_t cursor;
    int i;

    if (payload == NULL || payload_len == NULL) {
        return 0;
    }

    length = 2u + (size_t)DIRECT_REPEAT_COUNT * 7u + 4u;
    data = (unsigned char *)malloc(length);
    if (data == NULL) {
        return 0;
    }

    cursor = 0u;
    data[cursor++] = '#';
    data[cursor++] = '1';
    for (i = 0; i < DIRECT_REPEAT_COUNT; ++i) {
        data[cursor++] = '!';
        data[cursor++] = '6';
        data[cursor++] = '5';
        data[cursor++] = '5';
        data[cursor++] = '3';
        data[cursor++] = '5';
        data[cursor++] = '?';
    }
    data[cursor++] = '-';
    data[cursor++] = '#';
    data[cursor++] = '1';
    data[cursor++] = '@';

    *payload = data;
    *payload_len = (int)cursor;
    return 1;
}

static int
direct_repeat_bytes_equal(unsigned char const *data,
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
test_decoder_0019_decoder_parallel_direct_repeat_overflow_fallback(
    int argc,
    char **argv)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    image_buffer_t *image;
    unsigned char *payload;
    size_t pixels;
    int payload_len;
    int ok;

    (void)argc;
    (void)argv;

    allocator = NULL;
    image = NULL;
    payload = NULL;
    pixels = (size_t)DIRECT_REPEAT_WIDTH *
        (size_t)DIRECT_REPEAT_HEIGHT;
    payload_len = 0;
    ok = 0;

    if (!direct_repeat_make_payload(&payload, &payload_len)) {
        goto end;
    }

    status = sixel_decoder_parallel_override_threads("2");
    if (SIXEL_FAILED(status)) {
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
                               DIRECT_REPEAT_WIDTH,
                               DIRECT_REPEAT_HEIGHT,
                               0,
                               1,
                               1,
                               allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    memset(image->pixels.p, DIRECT_REPEAT_PIXEL_SENTINEL, pixels);
    memset(image->paint_mask, DIRECT_REPEAT_MASK_SENTINEL, pixels);

    status = sixel_decoder_parallel_request_start(
        0,
        0,
        payload,
        payload_len,
        payload,
        image,
        1,
        image->palette,
        NULL,
        0U,
        NULL,
        NULL);
    if (status != SIXEL_FALSE) {
        fprintf(stderr,
                "repeat overflow returned %04x, expected SIXEL_FALSE\n",
                status);
        goto end;
    }

    if (!direct_repeat_bytes_equal(image->pixels.p,
                                   pixels,
                                   DIRECT_REPEAT_PIXEL_SENTINEL)) {
        fprintf(stderr, "repeat overflow dirtied indexed pixels\n");
        goto end;
    }
    if (!direct_repeat_bytes_equal(image->paint_mask,
                                   pixels,
                                   DIRECT_REPEAT_MASK_SENTINEL)) {
        fprintf(stderr, "repeat overflow dirtied paint mask\n");
        goto end;
    }

    ok = 1;

end:
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
    free(payload);
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
