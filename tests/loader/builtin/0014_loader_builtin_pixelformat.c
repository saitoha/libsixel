/*
 * Verify builtin loader reports expected pixelformats:
 * - RGBA(8-bit)  -> RGB888
 * - Gray(16-bit) -> RGBFLOAT32 (no 8-bit precision loss)
 */

#include "tests/loader/pixelformat_test_common.h"

static SIXELSTATUS
new_builtin_component(sixel_allocator_t *allocator,
                      sixel_loader_component_t **ppcomponent)
{
    return create_loader_component_by_name("builtin", allocator, ppcomponent);
}

static int
run_builtin_loader_test(void)
{
    int result;

    result = run_loader_component_case("builtin loader rgba8",
                                       RGBA_IMAGE_PATH,
                                       SIXEL_PIXELFORMAT_RGB888,
                                       2,
                                       1,
                                       new_builtin_component);
    if (result != 0) {
        return result;
    }

    return run_loader_component_case("builtin loader gray16",
                                     "/tests/data/inputs/formats/snake-png-gray16.png",
                                     SIXEL_PIXELFORMAT_RGBFLOAT32,
                                     GEOMETRY_ANY,
                                     GEOMETRY_ANY,
                                     new_builtin_component);
}

int
test_loader_0014_loader_builtin_pixelformat(int argc, char **argv)
{
    (void) argc;
    (void) argv;

    return run_builtin_loader_test();
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
