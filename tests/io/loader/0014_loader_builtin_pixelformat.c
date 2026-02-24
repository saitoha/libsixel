/*
 * Verify builtin loader reports RGB output for RGBA sources.
 */

#include "tests/io/loader/pixelformat_test_common.h"

#include "src/loader-builtin.h"

static int
run_builtin_loader_test(void)
{
    return run_loader_component_case("builtin loader",
                                     RGBA_IMAGE_PATH,
                                     SIXEL_PIXELFORMAT_RGB888,
                                     2,
                                     1,
                                     sixel_loader_builtin_new);
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
