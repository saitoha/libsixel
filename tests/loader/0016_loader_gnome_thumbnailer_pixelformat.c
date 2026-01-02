/*
 * Verify GNOME thumbnailer path reports RGB output for RGBA sources.
 */

#include "pixelformat_test_common.h"

#include "loader-gnome-thumbnailer.h"

#if HAVE_UNISTD_H && HAVE_SYS_WAIT_H && HAVE_FORK
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
    return run_loader_case("GNOME thumbnailer",
                           RGBA_IMAGE_PATH,
                           SIXEL_PIXELFORMAT_RGB888,
                           GEOMETRY_ANY,
                           GEOMETRY_ANY,
                           load_with_gnome_thumbnailer);
}
#endif

int
main(void)
{
#if HAVE_UNISTD_H && HAVE_SYS_WAIT_H && HAVE_FORK
    printf("1..1\n");

    if (thumbnailer_available() == 0) {
        printf("ok 1 - GNOME thumbnailer unavailable"
               " # SKIP gdk-pixbuf-thumbnailer missing or unusable\n");
        return 0;
    }

    return run_thumbnailer_loader_test();
#else
    printf("1..0 # SKIP GNOME thumbnailer unavailable\n");
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
