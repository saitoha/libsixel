/*
 * Verify CoreGraphics output policy for alpha, indexed, and high-depth paths.
 */

#include "tests/loader/pixelformat_test_common.h"

#if HAVE_COREGRAPHICS
static SIXELSTATUS
new_coregraphics_component(sixel_allocator_t *allocator,
                           sixel_loader_component_t **ppcomponent)
{
    return create_loader_component_by_name("coregraphics",
                                           allocator,
                                           ppcomponent);
}

static int
run_coregraphics_loader_test(void)
{
    static unsigned char const white_bg[3] = { 255u, 255u, 255u };
    loader_component_case_spec_t const specs[] = {
        {
            "coregraphics rgba no background emits rgb+mask",
            RGBA_IMAGE_PATH,
            {
                SIXEL_PIXELFORMAT_RGB888,
                2,
                1,
                1,
                -1,
                FRAME_METADATA_ANY,
                1,
                1
            },
            { 1, 0, 256, NULL },
            new_coregraphics_component
        },
        {
            "coregraphics rgba with background emits rgb",
            RGBA_IMAGE_PATH,
            {
                SIXEL_PIXELFORMAT_RGB888,
                2,
                1,
                1,
                -1,
                FRAME_METADATA_ANY,
                0,
                0
            },
            { 1, 0, 256, white_bg },
            new_coregraphics_component
        },
        {
            "coregraphics indexed png keeps pal8",
            "/tests/data/inputs/formats/snake-png-pal8.png",
            {
                SIXEL_PIXELFORMAT_PAL8,
                64,
                64,
                1,
                -1,
                FRAME_METADATA_ANY,
                0,
                0
            },
            { 1, 1, 256, NULL },
            new_coregraphics_component
        },
        {
            "coregraphics indexed keycolor keeps pal8+transparent",
            "/tests/data/inputs/formats/pal8-trns-key0.png",
            {
                SIXEL_PIXELFORMAT_PAL8,
                4,
                1,
                1,
                FRAME_TRANSPARENT_NONNEG,
                FRAME_METADATA_ANY,
                0,
                0
            },
            { 1, 1, 256, NULL },
            new_coregraphics_component
        },
        {
            "coregraphics indexed keycolor reqcolors fallback rgb+mask",
            "/tests/data/inputs/formats/pal8-trns-key0.png",
            {
                SIXEL_PIXELFORMAT_RGB888,
                4,
                1,
                1,
                -1,
                FRAME_METADATA_ANY,
                1,
                1
            },
            { 1, 1, 3, NULL },
            new_coregraphics_component
        },
        {
            "coregraphics indexed reqcolors fallback emits rgb",
            "/tests/data/inputs/formats/snake-png-pal8.png",
            {
                SIXEL_PIXELFORMAT_RGB888,
                64,
                64,
                1,
                -1,
                FRAME_METADATA_ANY,
                0,
                0
            },
            { 1, 1, 253, NULL },
            new_coregraphics_component
        },
        {
            "coregraphics indexed alpha fallback emits rgb+mask",
            "/tests/data/inputs/formats/libpng-pal8-trns-single0-semi-icc.png",
            {
                SIXEL_PIXELFORMAT_RGB888,
                6,
                1,
                1,
                -1,
                FRAME_METADATA_ANY,
                1,
                1
            },
            { 1, 1, 256, NULL },
            new_coregraphics_component
        },
        {
            "coregraphics indexed alpha with background composites",
            "/tests/data/inputs/formats/libpng-pal8-trns-single0-semi-icc.png",
            {
                SIXEL_PIXELFORMAT_RGB888,
                6,
                1,
                1,
                -1,
                FRAME_METADATA_ANY,
                0,
                0
            },
            { 1, 1, 256, white_bg },
            new_coregraphics_component
        },
        {
            "coregraphics high-depth png promotes to float32",
            "/tests/data/inputs/formats/snake-png-gray16.png",
            {
                SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
                64,
                64,
                1,
                -1,
                FRAME_METADATA_ANY,
                0,
                0
            },
            { 1, 0, 256, NULL },
            new_coregraphics_component
        }
    };
    size_t index;
    int result;

    index = 0u;
    result = 0;
    for (index = 0u; index < sizeof(specs) / sizeof(specs[0]); ++index) {
        result = run_loader_component_case_from_spec(&specs[index]);
        if (result != 0) {
            return result;
        }
    }

    return 0;
}
#endif

int
test_loader_0008_loader_coregraphics_pixelformat(int argc, char **argv)
{
    (void) argc;
    (void) argv;

#if HAVE_COREGRAPHICS
    return run_coregraphics_loader_test();
#else
    fprintf(stderr, "CoreGraphics loader unavailable\n");
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
