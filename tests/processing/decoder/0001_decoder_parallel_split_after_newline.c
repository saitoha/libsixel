/*
 * SPDX-License-Identifier: MIT
 *
 * Regression test for parallel decoder byte spans that begin immediately after
 * DECGNL.  A skipped span boundary leaves one sixel band, i.e. six scanlines,
 * black even though the serial decoder paints it.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "src/decoder-parallel.h"

typedef struct decoder_parallel_image {
    sixel_allocator_t *allocator;
    unsigned char *pixels;
    int width;
    int height;
} decoder_parallel_image_t;

static void
decoder_parallel_image_dispose(decoder_parallel_image_t *image)
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
decoder_parallel_decode_with_threads(char const *threads,
                                     decoder_parallel_image_t *image)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    unsigned char payload[] =
        "\033Pq\"1;1;12;12#1;2;100;100;100!12~-!12~-";
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

    /*
     * The payload after the final palette definition is exactly
     * "!12~-!12~-". With two decoder threads, the first worker's inclusive
     * stop offset lands on the first '-'. The second worker must therefore
     * start at the byte after that '-' instead of seeking to the next one.
     */
    status = sixel_decoder_parallel_override_threads(threads);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        allocator = NULL;
        goto end;
    }

    status = sixel_decode_direct(payload,
                                 (int)(sizeof(payload) - 1u),
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
decoder_parallel_images_match(decoder_parallel_image_t const *serial,
                              decoder_parallel_image_t const *parallel)
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
    if (serial->width != 12 || serial->height != 12) {
        return 0;
    }

    bytes = (size_t)serial->width * (size_t)serial->height * 4u;
    return memcmp(serial->pixels, parallel->pixels, bytes) == 0;
}

int
test_decoder_0001_decoder_parallel_split_after_newline(int argc,
                                                       char **argv)
{
    decoder_parallel_image_t serial;
    decoder_parallel_image_t parallel;
    int success;

    (void)argc;
    (void)argv;

    memset(&serial, 0, sizeof(serial));
    memset(&parallel, 0, sizeof(parallel));
    success = 0;

    if (!decoder_parallel_decode_with_threads("1", &serial)) {
        fprintf(stderr, "serial decoder setup failed\n");
        goto end;
    }
    if (!decoder_parallel_decode_with_threads("2", &parallel)) {
        fprintf(stderr, "parallel decoder setup failed\n");
        goto end;
    }
    if (!decoder_parallel_images_match(&serial, &parallel)) {
        fprintf(stderr, "parallel decoder skipped a sixel band\n");
        goto end;
    }

    success = 1;

end:
    decoder_parallel_image_dispose(&parallel);
    decoder_parallel_image_dispose(&serial);
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
