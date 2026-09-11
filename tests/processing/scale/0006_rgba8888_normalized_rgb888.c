/*
 * SPDX-License-Identifier: MIT
 *
 * Verify that the byte scaler uses the RGB888 depth produced by RGBA8888
 * normalization and leaves storage beyond the RGB destination untouched.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include <sixel.h>

int
test_scale_0006_rgba_rgb888(int argc, char **argv)
{
    static unsigned char const src[8] = {
        0x10u, 0x20u, 0x30u, 0x40u,
        0x50u, 0x60u, 0x70u, 0x80u
    };
    static unsigned char const expected[8] = {
        0x10u, 0x20u, 0x30u,
        0x50u, 0x60u, 0x70u,
        0xa5u, 0xa5u
    };
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    unsigned char dst[8];

    (void)argc;
    (void)argv;

    allocator = NULL;
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        return EXIT_FAILURE;
    }

    memset(dst, 0xa5, sizeof(dst));
    status = sixel_helper_scale_image(dst,
                                      src,
                                      2,
                                      1,
                                      SIXEL_PIXELFORMAT_RGBA8888,
                                      2,
                                      1,
                                      SIXEL_RES_NEAREST,
                                      allocator);
    if (SIXEL_SUCCEEDED(status)
        && memcmp(dst, expected, sizeof(expected)) != 0) {
        status = SIXEL_LOGIC_ERROR;
    }

    sixel_allocator_unref(allocator);
    return SIXEL_SUCCEEDED(status) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
