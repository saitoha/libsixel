/*
 * Verify GD loader output policy for alpha, indexed, and GIF paths.
 */

#include <string.h>

#include "tests/loader/pixelformat_test_common.h"

#if HAVE_GD
static SIXELSTATUS
new_gd_component(sixel_allocator_t *allocator,
                 sixel_loader_component_t **ppcomponent)
{
    return create_loader_component_by_name("gd", allocator, ppcomponent);
}

typedef enum gd_pixelformat_case_id {
    GD_PIXELFORMAT_RGBA_NO_BG_MASK = 0,
    GD_PIXELFORMAT_RGBA_BG_FLOAT32,
    GD_PIXELFORMAT_INDEXED_PAL8,
    GD_PIXELFORMAT_INDEXED_KEYCOLOR_PAL8,
    GD_PIXELFORMAT_INDEXED_KEYCOLOR_REQCOLORS_MASK,
    GD_PIXELFORMAT_INDEXED_KEYCOLOR_BG_FLOAT32,
    GD_PIXELFORMAT_HIGHDEPTH_FLOAT32,
    GD_PIXELFORMAT_GIF_OPAQUE_PAL8,
    GD_PIXELFORMAT_GIF_TRANSPARENT_PAL8,
    GD_PIXELFORMAT_GIF_OPAQUE_RGB,
    GD_PIXELFORMAT_GIF_TRANSPARENT_RGBA,
    GD_PIXELFORMAT_GIF_TRANSPARENT_LOW_REQCOLORS_RGBA,
    GD_PIXELFORMAT_GIF_TRANSPARENT_LOW_REQCOLORS_BG_RGB,
    GD_PIXELFORMAT_GIF_NO_NETSCAPE_MULTIFRAME,
    GD_PIXELFORMAT_GIF_BGINDEX_OOB_PAL8,
    GD_PIXELFORMAT_GIF_NETSCAPE_UNKNOWN_SUBTYPE_METADATA,
    GD_PIXELFORMAT_GIF_TRANSPARENT_INDEX_OOB_OPAQUE,
    GD_PIXELFORMAT_CASE_COUNT
} gd_pixelformat_case_id_t;

static int
run_gd_pixelformat_case_by_id(gd_pixelformat_case_id_t case_id)
{
    static unsigned char const white_bg[3] = { 0xffu, 0xffu, 0xffu };
    static loader_component_case_spec_t const specs[] = {
        {
            "gd rgba no background emits rgb+mask",
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
            new_gd_component
        },
        {
            "gd rgba with background emits linear float32",
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
            new_gd_component
        },
        {
            "gd indexed png preserves pal8 when requested",
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
            { 0, 1, 256, NULL },
            new_gd_component
        },
        {
            "gd indexed keycolor preserves pal8+transparent index",
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
            { 0, 1, 256, NULL },
            new_gd_component
        },
        {
            "gd indexed keycolor reqcolors fallback emits rgb+mask",
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
            new_gd_component
        },
        {
            "gd indexed keycolor with background emits linear float32",
            "/tests/data/inputs/formats/pal8-trns-key0.png",
            {
                SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
                4,
                1,
                1,
                -1,
                FRAME_METADATA_ANY,
                0,
                0
            },
            { 1, 1, 3, white_bg },
            new_gd_component
        },
        {
            "gd high-depth png promotes to float32",
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
            new_gd_component
        },
        {
            "gd gif opaque pal8",
            "/tests/data/inputs/small.gif",
            {
                SIXEL_PIXELFORMAT_PAL8,
                GEOMETRY_ANY,
                GEOMETRY_ANY,
                1,
                -1,
                1,
                FRAME_MASK_ANY,
                FRAME_ALPHA_ZERO_ANY
            },
            { 1, 1, 256, NULL },
            new_gd_component
        },
        {
            "gd gif transparent pal8",
            "/tests/data/inputs/formats/gif-transparent-static.gif",
            {
                SIXEL_PIXELFORMAT_PAL8,
                8,
                8,
                1,
                FRAME_TRANSPARENT_NONNEG,
                0,
                FRAME_MASK_ANY,
                FRAME_ALPHA_ZERO_ANY
            },
            { 1, 1, 256, NULL },
            new_gd_component
        },
        {
            "gd gif opaque rgb",
            "/tests/data/inputs/small.gif",
            {
                SIXEL_PIXELFORMAT_RGB888,
                GEOMETRY_ANY,
                GEOMETRY_ANY,
                1,
                -1,
                1,
                FRAME_MASK_ANY,
                FRAME_ALPHA_ZERO_ANY
            },
            { 1, 0, 256, NULL },
            new_gd_component
        },
        {
            "gd gif transparent rgba",
            "/tests/data/inputs/formats/gif-transparent-static.gif",
            {
                SIXEL_PIXELFORMAT_RGBA8888,
                8,
                8,
                1,
                -1,
                0,
                FRAME_MASK_ANY,
                FRAME_ALPHA_ZERO_ANY
            },
            { 1, 0, 256, NULL },
            new_gd_component
        },
        {
            "gd gif transparent low reqcolors rgba fallback",
            "/tests/data/inputs/formats/gif-transparent-static-3colors.gif",
            {
                SIXEL_PIXELFORMAT_RGBA8888,
                8,
                8,
                1,
                -1,
                0,
                FRAME_MASK_ANY,
                FRAME_ALPHA_ZERO_ANY
            },
            { 1, 1, 3, NULL },
            new_gd_component
        },
        {
            "gd gif transparent low reqcolors rgb fallback with background",
            "/tests/data/inputs/formats/gif-transparent-static-3colors.gif",
            {
                SIXEL_PIXELFORMAT_RGB888,
                8,
                8,
                1,
                -1,
                0,
                FRAME_MASK_ANY,
                FRAME_ALPHA_ZERO_ANY
            },
            { 1, 1, 3, white_bg },
            new_gd_component
        },
        {
            "gd gif no-netscape multiframe metadata",
            "/tests/data/inputs/formats/gif-anim-no-netscape-2frame.gif",
            {
                SIXEL_PIXELFORMAT_PAL8,
                6,
                6,
                2,
                -1,
                1,
                FRAME_MASK_ANY,
                FRAME_ALPHA_ZERO_ANY
            },
            { 0, 1, 256, NULL },
            new_gd_component
        },
        {
            "gd gif bgindex-oob fallback pal8",
            "/tests/data/inputs/formats/gif-bgindex-oob-anim.gif",
            {
                SIXEL_PIXELFORMAT_PAL8,
                2,
                1,
                1,
                -1,
                1,
                FRAME_MASK_ANY,
                FRAME_ALPHA_ZERO_ANY
            },
            { 1, 1, 256, NULL },
            new_gd_component
        },
        {
            "gd gif netscape unknown subtype metadata",
            "/tests/data/inputs/formats/gif-anim-netscape-unknown-subtype.gif",
            {
                SIXEL_PIXELFORMAT_PAL8,
                2,
                1,
                2,
                -1,
                1,
                FRAME_MASK_ANY,
                FRAME_ALPHA_ZERO_ANY
            },
            { 0, 1, 256, NULL },
            new_gd_component
        },
        {
            "gd gif transparent-index-oob treated opaque",
            "/tests/data/inputs/formats/gif-transparent-index-oob-static.gif",
            {
                SIXEL_PIXELFORMAT_PAL8,
                8,
                8,
                1,
                -1,
                0,
                FRAME_MASK_ANY,
                FRAME_ALPHA_ZERO_ANY
            },
            { 1, 1, 256, NULL },
            new_gd_component
        }
    };
    size_t index;

    if (case_id < 0 ||
        case_id >= GD_PIXELFORMAT_CASE_COUNT ||
        (size_t)case_id >= sizeof(specs) / sizeof(specs[0])) {
        return 1;
    }

    index = (size_t)case_id;
    return run_loader_component_case_from_spec(&specs[index]);
}

static int
run_gd_loader_test_mode(char const *mode)
{
    size_t index;
    int result;

    index = 0u;
    result = 0;
    if (mode == NULL || strcmp(mode, "all") == 0) {
        for (index = 0u; index < (size_t)GD_PIXELFORMAT_CASE_COUNT; ++index) {
            result = run_gd_pixelformat_case_by_id(
                (gd_pixelformat_case_id_t)index);
            if (result != 0) {
                return result;
            }
        }
        return 0;
    }
    if (strcmp(mode, "rgba_no_bg_mask") == 0) {
        return run_gd_pixelformat_case_by_id(GD_PIXELFORMAT_RGBA_NO_BG_MASK);
    }
    if (strcmp(mode, "rgba_bg_float32") == 0) {
        return run_gd_pixelformat_case_by_id(GD_PIXELFORMAT_RGBA_BG_FLOAT32);
    }
    if (strcmp(mode, "indexed_pal8") == 0) {
        return run_gd_pixelformat_case_by_id(GD_PIXELFORMAT_INDEXED_PAL8);
    }
    if (strcmp(mode, "indexed_keycolor_pal8") == 0) {
        return run_gd_pixelformat_case_by_id(
            GD_PIXELFORMAT_INDEXED_KEYCOLOR_PAL8);
    }
    if (strcmp(mode, "indexed_keycolor_reqcolors_mask") == 0) {
        return run_gd_pixelformat_case_by_id(
            GD_PIXELFORMAT_INDEXED_KEYCOLOR_REQCOLORS_MASK);
    }
    if (strcmp(mode, "indexed_keycolor_bg_float32") == 0) {
        return run_gd_pixelformat_case_by_id(
            GD_PIXELFORMAT_INDEXED_KEYCOLOR_BG_FLOAT32);
    }
    if (strcmp(mode, "highdepth_float32") == 0) {
        return run_gd_pixelformat_case_by_id(
            GD_PIXELFORMAT_HIGHDEPTH_FLOAT32);
    }

    fprintf(stderr, "unknown gd pixelformat test mode: %s\n", mode);
    return 1;
}
#endif

int
test_loader_0011_loader_gd_pixelformat(int argc, char **argv)
{
    (void) argc;
    (void) argv;

#if HAVE_GD
    if (argc > 1 && argv != NULL) {
        return run_gd_loader_test_mode(argv[1]);
    }
    return run_gd_loader_test_mode(NULL);
#else
    fprintf(stderr, "GD loader unavailable\n");
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
