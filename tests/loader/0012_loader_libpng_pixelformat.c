/*
 * Verify libpng loader reports RGB output for RGBA sources.
 */

#include "pixelformat_test_common.h"

#include "loader-libpng.h"

#if HAVE_LIBPNG
static int
run_libpng_loader_test(void)
{
    return run_loader_case("libpng loader",
                           RGBA_IMAGE_PATH,
                           SIXEL_PIXELFORMAT_RGB888,
                           2,
                           1,
                           load_with_libpng);
}
#endif

int
main(void)
{
#if HAVE_LIBPNG
    printf("1..1\n");
    return run_libpng_loader_test();
#else
    printf("1..0 # SKIP libpng loader unavailable\n");
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
