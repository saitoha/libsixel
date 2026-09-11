/*
 * SPDX-License-Identifier: MIT
 *
 * Verify that the byte scaler uses the RGB888 depth produced by RGB565
 * normalization and writes every destination component without overrunning.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include <sixel.h>

int
test_scale_0005_rgb565_rgb888(int argc, char **argv)
{
    static unsigned char const src[4] = {
        0x00u, 0x00u,
        0xffu, 0xffu
    };
    static unsigned char const expected[8] = {
        0x00u, 0x00u, 0x00u,
        0xf8u, 0xfcu, 0xf8u,
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
                                      SIXEL_PIXELFORMAT_RGB565,
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
