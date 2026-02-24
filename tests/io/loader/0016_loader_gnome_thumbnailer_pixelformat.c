/*
 * Verify GNOME thumbnailer path reports RGB output for RGBA sources.
 */

#include "tests/io/loader/pixelformat_test_common.h"

#include "src/loader-gnome-thumbnailer.h"

#if HAVE_FREEDESKTOP_THUMBNAILING
static int
thumbnailer_available(void)
{
    int has_directories;
    int has_match;
    int has_fallback;

    has_directories = 0;
    has_match = 0;
    has_fallback = 0;

    loader_probe_gnome_thumbnailers("image/png",
                                    &has_directories,
                                    &has_match);

    if (has_directories == 0 || has_match == 0) {
        return 0;
    }

    has_fallback = thumbnailer_has_fallback_thumbnailer();
    if (has_fallback == 0) {
        return 0;
    }

    return 1;
}

static int
run_thumbnailer_loader_test(void)
{
    return run_loader_component_case("GNOME thumbnailer",
                                     RGBA_IMAGE_PATH,
                                     SIXEL_PIXELFORMAT_RGB888,
                                     GEOMETRY_ANY,
                                     GEOMETRY_ANY,
                                     sixel_loader_gnome_thumbnailer_new);
}
#endif

int
test_loader_0016_loader_gnome_thumbnailer_pixelformat(int argc, char **argv)
{
    (void) argc;
    (void) argv;

#if HAVE_FREEDESKTOP_THUMBNAILING
    if (thumbnailer_available() == 0) {
        fprintf(stderr,
                "GNOME thumbnailer unavailable: "
                "gdk-pixbuf-thumbnailer missing or unusable\n");
        return SIXEL_TEST_SKIP;
    }

    return run_thumbnailer_loader_test();
#else
    fprintf(stderr, "GNOME thumbnailer unavailable\n");
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
