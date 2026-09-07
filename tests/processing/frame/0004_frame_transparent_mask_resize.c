/*
 * SPDX-License-Identifier: MIT
 *
 * Policy: docs/concepts/pixelformat.md
 *
 * Verify the exact binary transparent mask produced by nearest-neighbor
 * frame resizing while retaining alpha-zero metadata.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include <sixel.h>

#include "tests/processing/frame/frame_transparent_mask_resize_test_common.h"

int
test_frame_0004_transparent_mask_resize(int argc, char **argv)
{
    static unsigned char const expected_mask[7] = {
        1u, 1u, 1u, 1u, 0u, 0u, 0u
    };

    (void)argc;
    (void)argv;
    if (!frame_transparent_mask_resize_run(SIXEL_RES_NEAREST,
                                           expected_mask)) {
        fprintf(stderr, "nearest transparent mask resize failed\n");
        return EXIT_FAILURE;
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
/* EOF */
