/*
 * Verify libjpeg loader reports RGB output for JPEG sources.
 */

#include "tests/loader/pixelformat_test_common.h"

#include "src/loader-libjpeg.h"

#if HAVE_JPEG
static int
run_libjpeg_loader_test(void)
{
    return run_loader_case("libjpeg loader",
                           JPEG_IMAGE_PATH,
                           SIXEL_PIXELFORMAT_RGB888,
                           600,
                           450,
                           load_with_libjpeg);
}
#endif

int
test_loader_0013_loader_libjpeg_pixelformat(int argc, char **argv)
{
    (void) argc;
    (void) argv;

#if HAVE_JPEG
    return run_libjpeg_loader_test();
#else
    fprintf(stderr, "libjpeg loader unavailable\n");
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
