/*
 * SPDX-License-Identifier: MIT
 *
 * Characterize the legacy animation cursor environment contract.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include "src/compat_stub.h"
#include "src/encoder.h"

static int
animation_hide_cursor_environment_matches(char const *text,
                                          int expected)
{
    char const *actual;

    if (sixel_compat_setenv("SIXEL_ANIMATION_HIDE_CURSOR", text) != 0) {
        return 0;
    }
    actual = sixel_compat_getenv("SIXEL_ANIMATION_HIDE_CURSOR");
    return sixel_encoder_should_hide_animation_cursor(1, 0, 1, actual)
        == expected;
}

int
test_loader_0062_loader_animation_hide_cursor_environment(int argc,
                                                           char **argv)
{
    (void)argc;
    (void)argv;

    if (!animation_hide_cursor_environment_matches("1", 1)) {
        fprintf(stderr, "one no longer enables animation cursor hiding\n");
        return EXIT_FAILURE;
    }
    if (!animation_hide_cursor_environment_matches("0", 0)) {
        fprintf(stderr, "zero unexpectedly enables animation cursor hiding\n");
        return EXIT_FAILURE;
    }
    if (!animation_hide_cursor_environment_matches("", 0)) {
        fprintf(stderr, "empty animation cursor policy changed\n");
        return EXIT_FAILURE;
    }
    if (!animation_hide_cursor_environment_matches("on", 0)) {
        fprintf(stderr, "non-numeric animation cursor policy changed\n");
        return EXIT_FAILURE;
    }

    (void)sixel_compat_setenv("SIXEL_ANIMATION_HIDE_CURSOR", "");
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
