/*
 * Verify GD loader pixelformats for PNG and shared GIF decode paths.
 */

#include "tests/loader/pixelformat_test_common.h"

#if HAVE_GD
static SIXELSTATUS
new_gd_component(sixel_allocator_t *allocator,
                 sixel_loader_component_t **ppcomponent)
{
    return create_loader_component_by_name("gd", allocator, ppcomponent);
}

static int
run_gd_loader_test(void)
{
    int result;

    result = run_loader_component_case("GD loader rgba8",
                                       RGBA_IMAGE_PATH,
                                       SIXEL_PIXELFORMAT_RGB888,
                                       2,
                                       1,
                                       new_gd_component);
    if (result != 0) {
        return result;
    }

    result = run_loader_component_case_with_options("GD loader gif opaque pal8",
                                                    "/tests/data/inputs/small.gif",
                                                    SIXEL_PIXELFORMAT_PAL8,
                                                    GEOMETRY_ANY,
                                                    GEOMETRY_ANY,
                                                    1,
                                                    1,
                                                    256,
                                                    new_gd_component);
    if (result != 0) {
        return result;
    }

    result = run_loader_component_case_with_options(
        "GD loader gif transparent pal8",
        "/tests/data/inputs/formats/gif-transparent-static.gif",
        SIXEL_PIXELFORMAT_PAL8,
        8,
        8,
        1,
        1,
        256,
        new_gd_component);
    if (result != 0) {
        return result;
    }

    result = run_loader_component_case_with_options("GD loader gif opaque rgb",
                                                    "/tests/data/inputs/small.gif",
                                                    SIXEL_PIXELFORMAT_RGB888,
                                                    GEOMETRY_ANY,
                                                    GEOMETRY_ANY,
                                                    1,
                                                    0,
                                                    256,
                                                    new_gd_component);
    if (result != 0) {
        return result;
    }

    return run_loader_component_case_with_options(
        "GD loader gif transparent rgba",
        "/tests/data/inputs/formats/gif-transparent-static.gif",
        SIXEL_PIXELFORMAT_RGBA8888,
        8,
        8,
        1,
        0,
        256,
        new_gd_component);
}
#endif

int
test_loader_0011_loader_gd_pixelformat(int argc, char **argv)
{
    (void) argc;
    (void) argv;

#if HAVE_GD
    return run_gd_loader_test();
#else
    fprintf(stderr, "GD loader unavailable\n");
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
