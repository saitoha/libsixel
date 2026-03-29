/*
 * Verify temporary loader background-colorspace override behavior.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include "src/loader-common.h"

int
test_loader_0053_loader_background_colorspace_override(int argc, char **argv)
{
    int base_colorspace;
    int current_colorspace;

    (void)argc;
    (void)argv;

    base_colorspace = loader_background_colorspace();
    current_colorspace = 0;

    sixel_helper_set_loader_background_colorspace(SIXEL_COLORSPACE_LINEAR);
    current_colorspace = loader_background_colorspace();
    if (current_colorspace != SIXEL_COLORSPACE_LINEAR) {
        fprintf(stderr, "background colorspace override to linear failed\n");
        sixel_helper_set_loader_background_colorspace(-1);
        return EXIT_FAILURE;
    }

    sixel_helper_set_loader_background_colorspace(SIXEL_COLORSPACE_GAMMA);
    current_colorspace = loader_background_colorspace();
    if (current_colorspace != SIXEL_COLORSPACE_GAMMA) {
        fprintf(stderr, "background colorspace override to gamma failed\n");
        sixel_helper_set_loader_background_colorspace(-1);
        return EXIT_FAILURE;
    }

    sixel_helper_set_loader_background_colorspace(12345);
    current_colorspace = loader_background_colorspace();
    if (current_colorspace != base_colorspace) {
        fprintf(stderr, "background colorspace override reset failed\n");
        sixel_helper_set_loader_background_colorspace(-1);
        return EXIT_FAILURE;
    }

    sixel_helper_set_loader_background_colorspace(-1);
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
