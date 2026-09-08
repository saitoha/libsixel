/*
 * SPDX-License-Identifier: MIT
 *
 * Policy: docs/concepts/pixelformat.md
 *
 * Verify that a transparent mask fences serial error diffusion from visible
 * neighboring pixels.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include <sixel.h>

#include "tests/processing/filter/filter_transparent_mask_fence_test_common.h"

int
test_filter_0043_filter_dither_transparent_mask_fence_serial(
    int argc,
    char **argv)
{
    (void)argc;
    (void)argv;

#if defined(_WIN32)
    return 77;
#else
    if (!filter_dither_transparent_mask_fence_run(0)) {
        fprintf(stderr, "transparent mask fence serial path failed\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
#endif
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
