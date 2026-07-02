/*
 * SPDX-License-Identifier: MIT
 *
 * Verify that sparse non-fast4 row chunks match serial decode when byte spans
 * are split across several sixel bands.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "src/decoder-parallel.h"

typedef struct sparse_split_image {
    sixel_allocator_t *allocator;
    unsigned char *pixels;
    int width;
    int height;
} sparse_split_image_t;

static unsigned char g_sparse_split_payload[] =
    "\033Pq\"1;1;48;48"
    "#1;2;100;0;0#2;2;0;100;0"
    "#3;2;0;0;100#4;2;100;100;0"
    "#1!48@$#2!12C!12?!12G!12?-"
    "#3!8A!8?!8Q!8?!8A!8?$#4!48B-"
    "#2!16H$#1!24@!24?-"
    "#4!48`$#3!6~!6?!6~!30?\033\\";

static void
sparse_split_image_dispose(sparse_split_image_t *image)
{
    if (image == NULL) {
        return;
    }
    if (image->allocator != NULL && image->pixels != NULL) {
        sixel_allocator_free(image->allocator, image->pixels);
    }
    if (image->allocator != NULL) {
        sixel_allocator_unref(image->allocator);
    }
    image->allocator = NULL;
    image->pixels = NULL;
    image->width = 0;
    image->height = 0;
}

static int
sparse_split_decode_with_threads(char const *threads,
                                 sparse_split_image_t *image)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    unsigned char *pixels;
    int width;
    int height;

    status = SIXEL_FALSE;
    allocator = NULL;
    pixels = NULL;
    width = 0;
    height = 0;

    if (threads == NULL || image == NULL) {
        goto end;
    }

    status = sixel_decoder_parallel_override_threads(threads);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        allocator = NULL;
        goto end;
    }

    status = sixel_decode_direct(g_sparse_split_payload,
                                 (int)(sizeof(g_sparse_split_payload) - 1U),
                                 &pixels,
                                 &width,
                                 &height,
                                 allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    image->allocator = allocator;
    image->pixels = pixels;
    image->width = width;
    image->height = height;
    allocator = NULL;
    pixels = NULL;

end:
    if (allocator != NULL) {
        if (pixels != NULL) {
            sixel_allocator_free(allocator, pixels);
        }
        sixel_allocator_unref(allocator);
    }
    return SIXEL_SUCCEEDED(status);
}

static int
sparse_split_images_match(sparse_split_image_t const *serial,
                          sparse_split_image_t const *parallel)
{
    size_t bytes;

    if (serial == NULL || parallel == NULL) {
        return 0;
    }
    if (serial->pixels == NULL || parallel->pixels == NULL) {
        return 0;
    }
    if (serial->width != parallel->width ||
            serial->height != parallel->height) {
        return 0;
    }
    if (serial->width != 48 || serial->height != 48) {
        return 0;
    }

    bytes = (size_t)serial->width * (size_t)serial->height * 4u;
    return memcmp(serial->pixels, parallel->pixels, bytes) == 0;
}

int
test_decoder_0017_decoder_parallel_sparse_split_matches_serial(int argc,
                                                               char **argv)
{
    sparse_split_image_t serial;
    sparse_split_image_t parallel;
    int success;

    (void)argc;
    (void)argv;

    memset(&serial, 0, sizeof(serial));
    memset(&parallel, 0, sizeof(parallel));
    success = 0;

    if (!sparse_split_decode_with_threads("1", &serial)) {
        fprintf(stderr, "serial decoder setup failed\n");
        goto end;
    }
    if (!sparse_split_decode_with_threads("4", &parallel)) {
        fprintf(stderr, "parallel decoder setup failed\n");
        goto end;
    }
    if (!sparse_split_images_match(&serial, &parallel)) {
        fprintf(stderr, "parallel sparse split differs from serial output\n");
        goto end;
    }

    success = 1;

end:
    (void)sixel_decoder_parallel_override_threads("1");
    sparse_split_image_dispose(&parallel);
    sparse_split_image_dispose(&serial);
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
