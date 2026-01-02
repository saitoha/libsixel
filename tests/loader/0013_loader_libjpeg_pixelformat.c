/*
 * Verify libjpeg loader reports RGB output for JPEG sources.
 */

#include "pixelformat_test_common.h"

#include "loader-libjpeg.h"

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
main(void)
{
#if HAVE_JPEG
    printf("1..1\n");
    return run_libjpeg_loader_test();
#else
    printf("1..0 # SKIP libjpeg loader unavailable\n");
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
