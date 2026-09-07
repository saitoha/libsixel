/*
 * SPDX-License-Identifier: MIT
 *
 * Policy: docs/concepts/pixelformat.md
 *
 * Verify that the builtin loader separates RGBA alpha-zero coverage into an
 * RGB frame plus a transparent mask when no background is resolved.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>

#include "tests/loader/pixelformat_test_common.h"

static SIXELSTATUS
new_builtin_alpha_mask_component(sixel_allocator_t *allocator,
                                 void **component_out)
{
    return create_loader_component_by_name("builtin",
                                           allocator,
                                           component_out);
}

int
test_loader_0063_loader_builtin_rgba_alpha_mask(int argc, char **argv)
{
    int result;

    (void)argc;
    (void)argv;
    result = run_loader_component_case_with_options_mask_ex(
        "builtin rgba alpha-zero mask representation",
        "/tests/data/inputs/formats/libpng-minimal-1x1-rgba.png",
        SIXEL_PIXELFORMAT_RGB888,
        1,
        1,
        1,
        (-1),
        0,
        1,
        1,
        1,
        1,
        0,
        256,
        NULL,
        new_builtin_alpha_mask_component);
    return result == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
