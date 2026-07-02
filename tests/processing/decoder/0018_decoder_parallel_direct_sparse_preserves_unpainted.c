/*
 * SPDX-License-Identifier: MIT
 *
 * Verify that the non-fast4 direct parallel path paints only touched spans
 * instead of copying zero-filled local rows over untouched pixels.
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

#define DIRECT_SPARSE_WIDTH 8
#define DIRECT_SPARSE_HEIGHT 12
#define DIRECT_SPARSE_PIXEL_SENTINEL 0xa5
#define DIRECT_SPARSE_MASK_SENTINEL 0x5a

static unsigned char g_direct_sparse_payload[] = {
    '#', '1', '@', '?', '?', '?', '?', '?',
    '?', '?', '-', '#', '1', '?'
};

int
test_decoder_0018_decoder_parallel_direct_sparse_preserves_unpainted(
    int argc,
    char **argv)
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
    pixels = (size_t)DIRECT_SPARSE_WIDTH *
        (size_t)DIRECT_SPARSE_HEIGHT;
    ok = 0;

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
                               DIRECT_SPARSE_WIDTH,
                               DIRECT_SPARSE_HEIGHT,
                               0,
                               1,
                               1,
                               allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    memset(image->pixels.p, DIRECT_SPARSE_PIXEL_SENTINEL, pixels);
    memset(image->paint_mask, DIRECT_SPARSE_MASK_SENTINEL, pixels);

    status = sixel_decoder_parallel_request_start(
        0,
        0,
        g_direct_sparse_payload,
        (int)sizeof(g_direct_sparse_payload),
        g_direct_sparse_payload,
        image,
        1,
        image->palette,
        NULL,
        0U,
        NULL,
        NULL);
    if (status != SIXEL_OK) {
        fprintf(stderr,
                "direct sparse request returned %04x\n",
                status);
        goto end;
    }

    if (image->pixels.in_bytes[0] != 1 ||
            image->paint_mask[0] != 0xffu) {
        fprintf(stderr, "direct sparse request did not paint x=0\n");
        goto end;
    }
    if (image->pixels.in_bytes[1] != DIRECT_SPARSE_PIXEL_SENTINEL ||
            image->paint_mask[1] != DIRECT_SPARSE_MASK_SENTINEL) {
        fprintf(stderr, "direct sparse request overwrote unpainted x=1\n");
        goto end;
    }
    if (image->pixels.in_bytes[DIRECT_SPARSE_WIDTH] !=
            DIRECT_SPARSE_PIXEL_SENTINEL ||
            image->paint_mask[DIRECT_SPARSE_WIDTH] !=
            DIRECT_SPARSE_MASK_SENTINEL) {
        fprintf(stderr, "direct sparse request overwrote unpainted row 1\n");
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
