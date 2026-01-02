/*
 * Verify CoreGraphics loads RGBA sources as four-component frames.
 */

#include "pixelformat_test_common.h"

#include "loader-coregraphics.h"

#if HAVE_COREGRAPHICS
static int
run_coregraphics_loader_test(void)
{
    return run_loader_case("CoreGraphics loader",
                           RGBA_IMAGE_PATH,
                           SIXEL_PIXELFORMAT_RGBA8888,
                           2,
                           1,
                           load_with_coregraphics);
}
#endif

int
main(void)
{
#if HAVE_COREGRAPHICS
    printf("1..1\n");
    return run_coregraphics_loader_test();
#else
    printf("1..0 # SKIP CoreGraphics loader unavailable\n");
    return 0;
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
