/*
 * Verify QuickLook loader reports RGBA output for RGBA sources.
 */

#include "pixelformat_test_common.h"

#include "loader-quicklook.h"

#if HAVE_COREGRAPHICS && HAVE_QUICKLOOK
static int
run_quicklook_loader_test(void)
{
    return run_loader_case("QuickLook loader",
                           RGBA_IMAGE_PATH,
                           SIXEL_PIXELFORMAT_RGBA8888,
                           GEOMETRY_ANY,
                           GEOMETRY_ANY,
                           load_with_quicklook);
}
#endif

int
main(void)
{
#if HAVE_COREGRAPHICS && HAVE_QUICKLOOK
    printf("1..1\n");
    return run_quicklook_loader_test();
#else
    printf("1..0 # SKIP QuickLook loader unavailable\n");
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
