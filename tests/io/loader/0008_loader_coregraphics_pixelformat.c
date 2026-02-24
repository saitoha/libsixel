/*
 * Verify CoreGraphics loads RGBA sources as four-component frames.
 */

#include "tests/io/loader/pixelformat_test_common.h"

#include "src/loader-coregraphics.h"

#if HAVE_COREGRAPHICS
static int
run_coregraphics_loader_test(void)
{
    return run_loader_component_case("CoreGraphics loader",
                                     RGBA_IMAGE_PATH,
                                     SIXEL_PIXELFORMAT_RGBA8888,
                                     2,
                                     1,
                                     sixel_loader_coregraphics_new);
}
#endif

int
test_loader_0008_loader_coregraphics_pixelformat(int argc, char **argv)
{
    (void) argc;
    (void) argv;

#if HAVE_COREGRAPHICS
    return run_coregraphics_loader_test();
#else
    fprintf(stderr, "CoreGraphics loader unavailable\n");
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
