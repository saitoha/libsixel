/*
 * Verify gdk-pixbuf2 output policy for alpha, indexed, and high-depth input.
 */

#include <string.h>

#include "tests/loader/pixelformat_test_common.h"

#if defined(HAVE_GDK_PIXBUF2)
static SIXELSTATUS
new_gdk_pixbuf_component(sixel_allocator_t *allocator,
                         sixel_loader_component_t **ppcomponent)
{
    return create_loader_component_by_name("gdk-pixbuf2",
                                           allocator,
                                           ppcomponent);
}

typedef enum gdk_pixbuf_pixelformat_case_id {
    GDK_PIXBUF_PIXELFORMAT_RGBA_NO_BG_MASK = 0,
    GDK_PIXBUF_PIXELFORMAT_RGBA_BG_FLOAT32,
    GDK_PIXBUF_PIXELFORMAT_RGBA_OPAQUE_BG_RGB888,
    GDK_PIXBUF_PIXELFORMAT_RGBA_OPAQUE_NO_BG_RGB888,
    GDK_PIXBUF_PIXELFORMAT_USE_PALETTE_NOOP,
    GDK_PIXBUF_PIXELFORMAT_REQCOLORS_NOOP,
    GDK_PIXBUF_PIXELFORMAT_INDEXED_RGB888,
    GDK_PIXBUF_PIXELFORMAT_INDEXED_KEYCOLOR_MASK,
    GDK_PIXBUF_PIXELFORMAT_INDEXED_KEYCOLOR_REQCOLORS_MASK,
    GDK_PIXBUF_PIXELFORMAT_INDEXED_ALPHA_MASK,
    GDK_PIXBUF_PIXELFORMAT_INDEXED_ALPHA_BG_FLOAT32,
    GDK_PIXBUF_PIXELFORMAT_HIGHDEPTH_FLOAT32,
    GDK_PIXBUF_PIXELFORMAT_CASE_COUNT
} gdk_pixbuf_pixelformat_case_id_t;

static int
run_gdk_pixbuf_pixelformat_case_by_id(
    gdk_pixbuf_pixelformat_case_id_t case_id)
{
    static unsigned char const white_bg[3] = { 255u, 255u, 255u };
    static loader_component_case_spec_t const specs[] = {
        {
            "gdkpixbuf rgba no background emits rgb+mask",
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
            new_gdk_pixbuf_component
        },
        {
            "gdkpixbuf rgba with background emits linear float32",
            RGBA_IMAGE_PATH,
            {
                SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
                2,
                1,
                1,
                -1,
                FRAME_METADATA_ANY,
                0,
                0
            },
            { 1, 0, 256, white_bg },
            new_gdk_pixbuf_component
        },
        {
            "gdkpixbuf opaque rgba with background emits rgb888 fast path",
            "/tests/data/inputs/formats/snake-64-reference-rgba.png",
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
            { 1, 0, 256, white_bg },
            new_gdk_pixbuf_component
        },
        {
            "gdkpixbuf opaque rgba without background emits rgb888 fast path",
            "/tests/data/inputs/formats/snake-64-reference-rgba.png",
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
            { 1, 0, 256, NULL },
            new_gdk_pixbuf_component
        },
        {
            "gdkpixbuf use-palette option is accepted but no-op",
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
            { 1, 1, 256, NULL },
            new_gdk_pixbuf_component
        },
        {
            "gdkpixbuf reqcolors option is accepted but no-op",
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
            { 1, 0, 2, NULL },
            new_gdk_pixbuf_component
        },
        {
            "gdkpixbuf indexed png emits rgb",
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
            { 1, 1, 256, NULL },
            new_gdk_pixbuf_component
        },
        {
            "gdkpixbuf indexed keycolor emits rgb+mask",
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
            { 1, 1, 256, NULL },
            new_gdk_pixbuf_component
        },
        {
            "gdkpixbuf indexed keycolor reqcolors fallback rgb+mask",
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
            new_gdk_pixbuf_component
        },
        {
            "gdkpixbuf indexed alpha fallback emits rgb+mask",
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
            new_gdk_pixbuf_component
        },
        {
            "gdkpixbuf indexed alpha with background emits linear float32",
            "/tests/data/inputs/formats/libpng-pal8-trns-single0-semi-icc.png",
            {
                SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
                6,
                1,
                1,
                -1,
                FRAME_METADATA_ANY,
                0,
                0
            },
            { 1, 1, 256, white_bg },
            new_gdk_pixbuf_component
        },
        {
            "gdkpixbuf high-depth png promotes to float32",
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
            new_gdk_pixbuf_component
        }
    };
    size_t index;

    if (case_id < 0 ||
        case_id >= GDK_PIXBUF_PIXELFORMAT_CASE_COUNT ||
        (size_t)case_id >= sizeof(specs) / sizeof(specs[0])) {
        return 1;
    }

    index = (size_t)case_id;
    return run_loader_component_case_from_spec(&specs[index]);
}

static int
run_gdk_pixbuf_loader_test_mode(char const *mode)
{
    if (mode == NULL || strcmp(mode, "rgba_no_bg_mask") == 0) {
        return run_gdk_pixbuf_pixelformat_case_by_id(
            GDK_PIXBUF_PIXELFORMAT_RGBA_NO_BG_MASK);
    }
    if (strcmp(mode, "rgba_bg_float32") == 0) {
        return run_gdk_pixbuf_pixelformat_case_by_id(
            GDK_PIXBUF_PIXELFORMAT_RGBA_BG_FLOAT32);
    }
    if (strcmp(mode, "rgba_opaque_bg_rgb888") == 0) {
        return run_gdk_pixbuf_pixelformat_case_by_id(
            GDK_PIXBUF_PIXELFORMAT_RGBA_OPAQUE_BG_RGB888);
    }
    if (strcmp(mode, "rgba_opaque_no_bg_rgb888") == 0) {
        return run_gdk_pixbuf_pixelformat_case_by_id(
            GDK_PIXBUF_PIXELFORMAT_RGBA_OPAQUE_NO_BG_RGB888);
    }
    if (strcmp(mode, "use_palette_noop") == 0) {
        return run_gdk_pixbuf_pixelformat_case_by_id(
            GDK_PIXBUF_PIXELFORMAT_USE_PALETTE_NOOP);
    }
    if (strcmp(mode, "reqcolors_noop") == 0) {
        return run_gdk_pixbuf_pixelformat_case_by_id(
            GDK_PIXBUF_PIXELFORMAT_REQCOLORS_NOOP);
    }
    if (strcmp(mode, "indexed_rgb888") == 0) {
        return run_gdk_pixbuf_pixelformat_case_by_id(
            GDK_PIXBUF_PIXELFORMAT_INDEXED_RGB888);
    }
    if (strcmp(mode, "indexed_keycolor_mask") == 0) {
        return run_gdk_pixbuf_pixelformat_case_by_id(
            GDK_PIXBUF_PIXELFORMAT_INDEXED_KEYCOLOR_MASK);
    }
    if (strcmp(mode, "indexed_keycolor_reqcolors_mask") == 0) {
        return run_gdk_pixbuf_pixelformat_case_by_id(
            GDK_PIXBUF_PIXELFORMAT_INDEXED_KEYCOLOR_REQCOLORS_MASK);
    }
    if (strcmp(mode, "indexed_alpha_mask") == 0) {
        return run_gdk_pixbuf_pixelformat_case_by_id(
            GDK_PIXBUF_PIXELFORMAT_INDEXED_ALPHA_MASK);
    }
    if (strcmp(mode, "indexed_alpha_bg_float32") == 0) {
        return run_gdk_pixbuf_pixelformat_case_by_id(
            GDK_PIXBUF_PIXELFORMAT_INDEXED_ALPHA_BG_FLOAT32);
    }
    if (strcmp(mode, "highdepth_float32") == 0) {
        return run_gdk_pixbuf_pixelformat_case_by_id(
            GDK_PIXBUF_PIXELFORMAT_HIGHDEPTH_FLOAT32);
    }
    if (strcmp(mode, "all") == 0) {
        size_t index;
        int result;

        index = 0u;
        result = 0;
        for (index = 0u;
             index < (size_t)GDK_PIXBUF_PIXELFORMAT_CASE_COUNT;
             ++index) {
            result = run_gdk_pixbuf_pixelformat_case_by_id(
                (gdk_pixbuf_pixelformat_case_id_t)index);
            if (result != 0) {
                return result;
            }
        }
        return 0;
    }

    fprintf(stderr, "unknown gdk-pixbuf2 pixelformat test mode: %s\n", mode);
    return 1;
}
#endif

int
test_loader_0010_loader_gdk_pixbuf_pixelformat(int argc, char **argv)
{
#if defined(HAVE_GDK_PIXBUF2)
    if (argc > 1 && argv != NULL) {
        return run_gdk_pixbuf_loader_test_mode(argv[1]);
    }
    return run_gdk_pixbuf_loader_test_mode(NULL);
#else
    (void) argc;
    (void) argv;
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
