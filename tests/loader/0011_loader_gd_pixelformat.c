/*
 * Verify GD loader reports RGB output for RGBA sources.
 */

#include "pixelformat_test_common.h"

#include "loader-gd.h"

#if HAVE_GD
static int
run_gd_loader_test(void)
{
    return run_loader_case("GD loader",
                           RGBA_IMAGE_PATH,
                           SIXEL_PIXELFORMAT_RGB888,
                           2,
                           1,
                           load_with_gd);
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
