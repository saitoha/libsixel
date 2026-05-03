/*
 * Regression test for GHSA-9jm7-77gr-qghv.
 *
 * This white-box PoC seeds the SIXEL parser cursor at the smallest value that
 * makes pos_x + repeat_count overflow in the DECSIXEL width calculation. The
 * '?' sixel has an empty bit mask, so the test exercises the horizontal
 * advance path without writing pixels after the vulnerable size check.
 */

#include "config.h"

#include <stdlib.h>

#if HAVE_LIMITS_H
# include <limits.h>
#endif  /* HAVE_LIMITS_H */

#include "../../../src/fromsixel.c"

int
main(void)
{
    int nret = EXIT_FAILURE;
    SIXELSTATUS status;
    sixel_allocator_t *allocator = NULL;
    parser_context_t context;
    image_buffer_t image;
    unsigned char sixel = '?';

    image.data = NULL;

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = parser_context_init(&context);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = image_buffer_init(&image, 1, 1, context.bgindex, allocator);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    context.state = PS_DECSIXEL;
    context.pos_x = INT_MAX - 4;
    context.repeat_count = 10;

    status = sixel_decode_raw_impl(&sixel, 1, &image, &context, allocator);
    if (status != SIXEL_BAD_INPUT) {
        goto error;
    }

    nret = EXIT_SUCCESS;

error:
    if (image.data != NULL && allocator != NULL) {
        sixel_allocator_free(allocator, image.data);
    }
    if (allocator != NULL) {
        sixel_allocator_unref(allocator);
    }
    return nret;
}
