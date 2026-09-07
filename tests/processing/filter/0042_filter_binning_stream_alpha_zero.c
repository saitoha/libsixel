/*
 * SPDX-License-Identifier: MIT
 *
 * Policy: docs/concepts/pixelformat.md
 *
 * Verify that sample-stream binning excludes an alpha-zero sample when frame
 * metadata enables alpha-zero interpretation.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include "filter_binning_stream_alpha_zero_test_common.h"

int
test_filter_0042_filter_binning_stream_alpha_zero(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    if (!filter_binning_stream_alpha_zero_run(1, 2u, 2.0, 0)) {
        fprintf(stderr, "alpha-zero sample was not excluded\n");
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
