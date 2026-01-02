/*
 * Verify WIC loader reports RGBA output for RGBA sources.
 */

#include "pixelformat_test_common.h"

#include "loader-wic.h"

#if HAVE_WIC
static int
run_wic_loader_test(void)
{
    return run_loader_case("WIC loader",
                           RGBA_IMAGE_PATH,
                           SIXEL_PIXELFORMAT_RGBA8888,
                           2,
                           1,
                           load_with_wic);
}
#endif

int
main(void)
{
#if HAVE_WIC
    printf("1..1\n");
    return run_wic_loader_test();
#else
    printf("1..0 # SKIP WIC loader unavailable\n");
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
