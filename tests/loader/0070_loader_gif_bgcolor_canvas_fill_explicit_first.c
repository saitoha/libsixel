/*
 * SPDX-License-Identifier: MIT
 *
 * Policy: docs/loader/background-policy.md
 *
 * Verify that explicit_first uses an explicit black background to fill the
 * GIF canvas instead of the logical-screen background.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include "src/compat_stub.h"
#include "tests/loader/gif_bgcolor_canvas_fill_test_common.h"

int
test_loader_0070_loader_gif_bgcolor_canvas_fill_explicit_first(
    int argc,
    char **argv)
{
    static unsigned char const black[3] = { 0x00u, 0x00u, 0x00u };
    static unsigned char const expected[12] = {
        0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u
    };
    int result;

    (void)argc;
    (void)argv;
    result = 0;
    if (sixel_compat_setenv("SIXEL_BACKGROUND_POLICY",
                            "explicit_first") != 0) {
        fprintf(stderr, "failed to select explicit_first\n");
        return EXIT_FAILURE;
    }
    result = loader_gif_bgcolor_canvas_fill_run(black, expected);
    (void)sixel_compat_setenv("SIXEL_BACKGROUND_POLICY", "");
    if (result == 0) {
        fprintf(stderr, "explicit_first GIF canvas fill failed\n");
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
