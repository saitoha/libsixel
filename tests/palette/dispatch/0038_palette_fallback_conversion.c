/*
 * SPDX-License-Identifier: MIT
 *
 * Verify that packed 1-bit pixels are expanded correctly by the fallback.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>

#include <sixel.h>

int
test_palfb_0006_conversion(int argc, char **argv)
{
    unsigned char src[2];
    unsigned char dst[9];
    unsigned char expected[9];
    SIXELSTATUS status;
    int dst_pixelformat;
    int index;

    (void)argc;
    (void)argv;

    src[0] = 0xaau;
    src[1] = 0x80u;
    expected[0] = 1u;
    expected[1] = 0u;
    expected[2] = 1u;
    expected[3] = 0u;
    expected[4] = 1u;
    expected[5] = 0u;
    expected[6] = 1u;
    expected[7] = 0u;
    expected[8] = 1u;
    dst_pixelformat = SIXEL_PIXELFORMAT_G1;

    status = sixel_helper_normalize_pixelformat(
        dst,
        &dst_pixelformat,
        src,
        SIXEL_PIXELFORMAT_G1,
        9,
        1);
    if (SIXEL_FAILED(status) || dst_pixelformat != SIXEL_PIXELFORMAT_G8) {
        return EXIT_FAILURE;
    }
    for (index = 0; index < 9; ++index) {
        if (dst[index] != expected[index]) {
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
