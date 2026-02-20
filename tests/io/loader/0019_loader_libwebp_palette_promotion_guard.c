/*
 * Verify palette mode does not promote non-indexed WebP inputs to PAL8.
 */

#include "tests/io/loader/pixelformat_test_common.h"

#include "src/loader-libwebp.h"

#if HAVE_WEBP
static int
run_libwebp_palette_guard_test(void)
{
    int status;

    status = run_loader_case_with_options(
        "libwebp non-indexed static webp keeps rgb",
        WEBP_IMAGE_PATH,
        SIXEL_PIXELFORMAT_RGB888,
        64,
        64,
        1,
        1,
        256,
        load_with_libwebp);
    if (status != 0) {
        return status;
    }

    return run_loader_case_with_options(
        "libwebp non-indexed animated webp keeps rgb",
        "/tests/data/inputs/formats/animated-lossy-8x8-2frame-min.webp",
        SIXEL_PIXELFORMAT_RGB888,
        8,
        8,
        1,
        1,
        256,
        load_with_libwebp);
}
#endif

int
test_loader_0019_loader_libwebp_palette_promotion_guard(int argc, char **argv)
{
    (void)argc;
    (void)argv;

#if HAVE_WEBP
    return run_libwebp_palette_guard_test();
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
