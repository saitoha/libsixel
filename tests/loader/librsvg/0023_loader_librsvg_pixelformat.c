/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 *
 * Policy: docs/concepts/pixelformat.md
 *
 * Verify that the librsvg loader preserves RGBA storage when no background
 * is provided.
 *
 */

#include "tests/loader/pixelformat_test_common.h"

#if HAVE_LIBRSVG
static SIXELSTATUS
new_librsvg_component(sixel_allocator_t *allocator,
                      void **ppcomponent)
{
    return create_loader_component_by_name("librsvg", allocator, ppcomponent);
}

static int
run_librsvg_pixelformat_test(void)
{
    return run_loader_component_case_with_options_ex(
        "librsvg keeps rgba without bgcolor",
        "/tests/data/inputs/formats/librsvg-transparent-2color.svg",
        SIXEL_PIXELFORMAT_RGBA8888,
        2,
        1,
        1,
        FRAME_METADATA_ANY,
        FRAME_METADATA_ANY,
        1,
        0,
        256,
        NULL,
        new_librsvg_component);
}

#endif

int
test_loader_0023_loader_librsvg_pixelformat(int argc, char **argv)
{
#if HAVE_LIBRSVG
    (void)argc;
    (void)argv;
    return run_librsvg_pixelformat_test();
#else
    (void)argc;
    (void)argv;
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
