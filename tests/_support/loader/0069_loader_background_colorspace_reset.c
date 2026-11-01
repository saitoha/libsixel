/*
 * SPDX-License-Identifier: MIT
 *
 * Policy: docs/loader/background-policy.md
 *
 * Verify that an invalid temporary background-colorspace value clears the
 * override and restores the configured base value.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include "src/loader-common.h"

int
test_loader_0069_loader_background_colorspace_reset(int argc, char **argv)
{
    int base_colorspace;
    int current_colorspace;

    (void)argc;
    (void)argv;
    base_colorspace = loader_background_colorspace();
    current_colorspace = 0;
    sixel_helper_set_loader_background_colorspace(SIXEL_COLORSPACE_LINEAR);
    sixel_helper_set_loader_background_colorspace(12345);
    current_colorspace = loader_background_colorspace();
    sixel_helper_set_loader_background_colorspace(-1);
    if (current_colorspace != base_colorspace) {
        fprintf(stderr, "background colorspace override reset failed\n");
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
