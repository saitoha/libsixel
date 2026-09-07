/*
 * SPDX-License-Identifier: MIT
 *
 * Policy: docs/loader/background-policy.md
 *
 * Verify the temporary gamma loader background-colorspace override.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include "src/loader-common.h"

int
test_loader_0068_loader_background_colorspace_gamma(int argc, char **argv)
{
    int current_colorspace;

    (void)argc;
    (void)argv;
    current_colorspace = 0;
    sixel_helper_set_loader_background_colorspace(SIXEL_COLORSPACE_GAMMA);
    current_colorspace = loader_background_colorspace();
    sixel_helper_set_loader_background_colorspace(-1);
    if (current_colorspace != SIXEL_COLORSPACE_GAMMA) {
        fprintf(stderr, "background colorspace override to gamma failed\n");
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
