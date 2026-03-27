/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 *
 * Verify librsvg loader output policy:
 * - preserve RGBA when background is not provided
 * - composite to RGB when background is provided
 * - keep RGB/RGBA output even when palette-related options are set
 */

#include "tests/loader/pixelformat_test_common.h"

#if HAVE_LIBRSVG
static SIXELSTATUS
new_librsvg_component(sixel_allocator_t *allocator,
                      sixel_loader_component_t **ppcomponent)
{
    return create_loader_component_by_name("librsvg", allocator, ppcomponent);
}

static int
run_librsvg_loader_test(void)
{
    int result;
    unsigned char white_bg[3];

    result = 0;
    white_bg[0] = 255u;
    white_bg[1] = 255u;
    white_bg[2] = 255u;

    result = run_loader_component_case_with_options_ex(
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
    if (result != 0) {
        return result;
    }

    result = run_loader_component_case_with_options_ex(
        "librsvg ignores palette options and keeps rgba",
        "/tests/data/inputs/formats/librsvg-transparent-2color.svg",
        SIXEL_PIXELFORMAT_RGBA8888,
        2,
        1,
        1,
        FRAME_METADATA_ANY,
        FRAME_METADATA_ANY,
        1,
        1,
        2,
        NULL,
        new_librsvg_component);
    if (result != 0) {
        return result;
    }

    result = run_loader_component_case_with_options_ex(
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
        new_librsvg_component);
    if (result != 0) {
        return result;
    }

    return run_loader_component_case_with_options_ex(
        "librsvg keeps rgb with bgcolor even when palette options are set",
        "/tests/data/inputs/formats/librsvg-transparent-2color.svg",
        SIXEL_PIXELFORMAT_RGB888,
        2,
        1,
        1,
        FRAME_METADATA_ANY,
        FRAME_METADATA_ANY,
        1,
        1,
        2,
        white_bg,
        new_librsvg_component);
}
#endif

int
test_loader_0023_loader_librsvg_pixelformat(int argc, char **argv)
{
    (void)argc;
    (void)argv;

#if HAVE_LIBRSVG
    return run_librsvg_loader_test();
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
