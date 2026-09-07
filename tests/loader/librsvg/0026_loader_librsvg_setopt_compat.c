/*
 * SPDX-License-Identifier: MIT
 *
 * Verify that librsvg palette compatibility options remain no-ops when an
 * explicit background is applied.
 */

#include "tests/loader/pixelformat_test_common.h"

#if HAVE_LIBRSVG
static SIXELSTATUS
new_librsvg_setopt_component(sixel_allocator_t *allocator,
                             void **component_out)
{
    return create_loader_component_by_name("librsvg",
                                           allocator,
                                           component_out);
}

static int
run_librsvg_setopt_compat_test(void)
{
    unsigned char white_bg[3];

    white_bg[0] = 255u;
    white_bg[1] = 255u;
    white_bg[2] = 255u;

    return run_loader_component_case_with_options_ex(
        "librsvg bgcolor works with palette options",
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
        new_librsvg_setopt_component);
}
#endif

int
test_loader_0026_loader_librsvg_setopt_compat(int argc, char **argv)
{
    (void)argc;
    (void)argv;
#if HAVE_LIBRSVG
    return run_librsvg_setopt_compat_test();
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
