/*
 * SPDX-License-Identifier: MIT
 *
 * Verify that the parallel decoder worker can compose OR-mode bit planes into
 * the direct-color side-car index buffer.
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

static unsigned char g_ormode_parallel_payload[] =
    "#1!2~$#2!2@-#1!2@$#2!2~";

int
test_decoder_0007_decoder_ormode_parallel_request(int argc, char **argv)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    image_buffer_t *image;
    size_t index_bytes;
    int ok;
    int x;
    int y;
    int expected;
    int actual;

    (void)argc;
    (void)argv;

    allocator = NULL;
    image = NULL;
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

    status = image_buffer_init(image, 2, 12, 0, 4, 0, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    index_bytes = (size_t)image->width * (size_t)image->height *
        sizeof(*image->ormode_indexes);
    image->ormode_indexes =
        (unsigned short *)sixel_allocator_malloc(allocator, index_bytes);
    if (image->ormode_indexes == NULL) {
        goto end;
    }
    memset(image->ormode_indexes, 0, index_bytes);

    status = sixel_decoder_parallel_request_start(
        1,
        1,
        g_ormode_parallel_payload,
        (int)(sizeof(g_ormode_parallel_payload) - 1U),
        g_ormode_parallel_payload,
        image,
        1,
        image->palette,
        NULL,
        0U,
        NULL,
        NULL);
    if (status != SIXEL_OK) {
        fprintf(stderr, "OR-mode parallel request returned %04x\n", status);
        goto end;
    }

    for (y = 0; y < image->height; ++y) {
        if (y == 0 || y == 6) {
            expected = 3;
        } else if (y < 6) {
            expected = 1;
        } else {
            expected = 2;
        }
        for (x = 0; x < image->width; ++x) {
            actual = image->ormode_indexes[y * image->width + x];
            if (actual != expected) {
                fprintf(stderr,
                        "OR-mode index at %d,%d is %d, expected %d\n",
                        x,
                        y,
                        actual,
                        expected);
                goto end;
            }
        }
    }

    ok = 1;

end:
    (void)sixel_decoder_parallel_override_threads("1");
    if (image != NULL) {
        if (allocator != NULL && image->ormode_indexes != NULL) {
            sixel_allocator_free(allocator, image->ormode_indexes);
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
