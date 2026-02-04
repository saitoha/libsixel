/*
 * Verify libwebp loader reports RGB output for WebP sources.
 */

#include "tests/io/loader/pixelformat_test_common.h"

#include "src/loader-libwebp.h"

#if HAVE_WEBP
static int
run_libwebp_loader_test(void)
{
    return run_loader_case("libwebp loader",
                           WEBP_IMAGE_PATH,
                           SIXEL_PIXELFORMAT_RGB888,
                           64,
                           64,
                           load_with_libwebp);
}
#endif

int
test_loader_0017_loader_libwebp_pixelformat(int argc, char **argv)
{
    (void) argc;
    (void) argv;

#if HAVE_WEBP
    return run_libwebp_loader_test();
#else
    fprintf(stderr, "libwebp loader unavailable\n");
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
