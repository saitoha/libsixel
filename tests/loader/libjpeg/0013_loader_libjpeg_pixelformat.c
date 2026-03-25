/*
 * Verify libjpeg loader reports float output for JPEG sources.
 */

#include "tests/loader/pixelformat_test_common.h"

#if HAVE_JPEG
static SIXELSTATUS
new_libjpeg_component(sixel_allocator_t *allocator,
                      sixel_loader_component_t **ppcomponent)
{
    return create_loader_component_by_name("libjpeg", allocator, ppcomponent);
}

static int
run_libjpeg_loader_test(void)
{
    int rc;

    rc = run_loader_component_case("libjpeg loader (8-bit grayscale)",
                                   JPEG_IMAGE_PATH,
                                   SIXEL_PIXELFORMAT_RGBFLOAT32,
                                   600,
                                   450,
                                   new_libjpeg_component);
    if (rc != 0) {
        return rc;
    }

#if defined(HAVE_JPEG12_API) && HAVE_JPEG12_API
    rc = run_loader_component_case("libjpeg loader (12-bit CMYK)",
                                   "/tests/data/inputs/formats/snake-jpeg-12bit-cmyk-seq444.jpg",
                                   SIXEL_PIXELFORMAT_RGBFLOAT32,
                                   64,
                                   64,
                                   new_libjpeg_component);
    if (rc != 0) {
        return rc;
    }

    rc = run_loader_component_case("libjpeg loader (12-bit YCCK)",
                                   "/tests/data/inputs/formats/snake-jpeg-12bit-ycck-seq444.jpg",
                                   SIXEL_PIXELFORMAT_RGBFLOAT32,
                                   64,
                                   64,
                                   new_libjpeg_component);
    if (rc != 0) {
        return rc;
    }
#endif

#if defined(HAVE_JPEG16_API) && HAVE_JPEG16_API
    rc = run_loader_component_case("libjpeg loader (16-bit CMYK)",
                                   "/tests/data/inputs/formats/snake-jpeg-16bit-cmyk-lossless.jpg",
                                   SIXEL_PIXELFORMAT_RGBFLOAT32,
                                   64,
                                   64,
                                   new_libjpeg_component);
    if (rc != 0) {
        return rc;
    }
#endif

    return 0;
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
