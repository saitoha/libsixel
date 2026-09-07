/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 *
 * Policy: docs/concepts/pixelformat.md
 *
 * Verify that the librsvg loader composites to RGB storage when an explicit
 * background is provided.
 */

#include <stdio.h>

#include "tests/loader/pixelformat_test_common.h"

#if HAVE_LIBRSVG
static SIXELSTATUS
new_librsvg_bgcolor_component(sixel_allocator_t *allocator,
                              void **component_out)
{
    return create_loader_component_by_name("librsvg",
                                           allocator,
                                           component_out);
}
#endif

int
test_loader_0033_loader_librsvg_bgcolor_pixelformat(int argc, char **argv)
{
#if HAVE_LIBRSVG
    unsigned char white_bg[3];
#endif

    (void)argc;
    (void)argv;
#if HAVE_LIBRSVG
    white_bg[0] = 255u;
    white_bg[1] = 255u;
    white_bg[2] = 255u;
    return run_loader_component_case_with_options_ex(
        "librsvg composites rgb with bgcolor",
        "/tests/data/inputs/formats/librsvg-transparent-2color.svg",
        SIXEL_PIXELFORMAT_RGB888,
        2,
        1,
        1,
        FRAME_METADATA_ANY,
        FRAME_METADATA_ANY,
        1,
        0,
        256,
        white_bg,
        new_librsvg_bgcolor_component);
#else
    fprintf(stderr, "librsvg loader unavailable\n");
    return SIXEL_TEST_SKIP;
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
