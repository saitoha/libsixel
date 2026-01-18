/*
 * Verify GDK-Pixbuf loader reports RGBA output for RGBA sources.
 */

#include "tests/loader/pixelformat_test_common.h"

#include "src/loader-gdk-pixbuf2.h"

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
test_loader_0010_loader_gdk_pixbuf_pixelformat(int argc, char **argv)
{
    (void) argc;
    (void) argv;

#if defined(HAVE_GDK_PIXBUF2)
    return run_gdk_pixbuf_loader_test();
#else
    fprintf(stderr, "GDK-Pixbuf loader unavailable\n");
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
