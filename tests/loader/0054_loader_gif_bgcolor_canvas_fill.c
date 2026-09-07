/*
 * SPDX-License-Identifier: MIT
 *
 * Policy: docs/loader/background-policy.md
 *
 * Verify that file_first uses the GIF logical-screen background to fill the
 * uncovered canvas instead of the explicit black background.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include "src/compat_stub.h"
#include "tests/loader/gif_bgcolor_canvas_fill_test_common.h"

int
test_loader_0054_loader_gif_bgcolor_canvas_fill(int argc, char **argv)
{
    static unsigned char const black[3] = { 0x00u, 0x00u, 0x00u };
    static unsigned char const expected[12] = {
        0x00u, 0x00u, 0x00u,
        0xffu, 0xffu, 0xffu,
        0xffu, 0xffu, 0xffu,
        0xffu, 0xffu, 0xffu
    };
    int result;

    (void)argc;
    (void)argv;
    result = 0;
    if (sixel_compat_setenv("SIXEL_BACKGROUND_POLICY", "file_first") != 0) {
        fprintf(stderr, "failed to select file_first\n");
        return EXIT_FAILURE;
    }
    result = loader_gif_bgcolor_canvas_fill_run(black, expected);
    (void)sixel_compat_setenv("SIXEL_BACKGROUND_POLICY", "");
    if (result == 0) {
        fprintf(stderr, "file_first GIF canvas fill failed\n");
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
