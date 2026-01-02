/*
 * Verify GDK-Pixbuf loader reports RGBA output for RGBA sources.
 */

#include "pixelformat_test_common.h"

#include "loader-gdk-pixbuf2.h"

#if defined(HAVE_GDK_PIXBUF2)
static int
run_gdk_pixbuf_loader_test(void)
{
    return run_loader_case("GDK-Pixbuf loader",
                           RGBA_IMAGE_PATH,
                           SIXEL_PIXELFORMAT_RGBA8888,
                           2,
                           1,
                           load_with_gdkpixbuf);
}
#endif

int
main(void)
{
#if defined(HAVE_GDK_PIXBUF2)
    printf("1..1\n");
    return run_gdk_pixbuf_loader_test();
#else
    printf("1..0 # SKIP GDK-Pixbuf loader unavailable\n");
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
