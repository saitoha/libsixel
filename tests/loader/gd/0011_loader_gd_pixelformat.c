/*
 * Verify GD loader output policy for alpha/indexed paths and colorspace.
 */

#include <string.h>

#include "tests/loader/pixelformat_test_common.h"

#if HAVE_GD
static SIXELSTATUS
new_gd_component(sixel_allocator_t *allocator,
                 void **ppcomponent)
{
    return create_loader_component_by_name("gd", allocator, ppcomponent);
}

typedef enum gd_pixelformat_case_id {
    GD_PIXELFORMAT_RGBA_NO_BG_MASK = 0,
    GD_PIXELFORMAT_RGBA_BG_FLOAT32,
    GD_PIXELFORMAT_INDEXED_PAL8,
    GD_PIXELFORMAT_OPAQUE_RGB_WITH_BG_STAYS_RGB888_GAMMA,
    GD_PIXELFORMAT_INDEXED_OPAQUE_WITH_BG_STAYS_RGB888_GAMMA,
    GD_PIXELFORMAT_INDEXED_KEYCOLOR_PAL8,
    GD_PIXELFORMAT_INDEXED_KEYCOLOR_REQCOLORS_MASK,
    GD_PIXELFORMAT_INDEXED_KEYCOLOR_BG_FLOAT32,
    GD_PIXELFORMAT_HIGHDEPTH_FLOAT32,
    GD_PIXELFORMAT_HIGHDEPTH_ALPHA_NO_BG_FLOAT32_MASK,
    GD_PIXELFORMAT_INDEXED_MULTI_TRNS_MASK,
    GD_PIXELFORMAT_INDEXED_KEYCOLOR_REQCOLORS_BOUNDARY_PAL8,
    GD_PIXELFORMAT_OPAQUE_RGB_GAMMA_FASTPATH,
    GD_PIXELFORMAT_INDEXED_NO_PALETTE_RGB_GAMMA,
    GD_PIXELFORMAT_INDEXED_KEYCOLOR_GAMA_ICCP_BG_FLOAT32_LINEAR,
    GD_PIXELFORMAT_GAMA_ONLY_BG_FLOAT32_LINEAR,
    GD_PIXELFORMAT_HIGHDEPTH_SRGB_ONLY_FLOAT32_LINEAR,
    GD_PIXELFORMAT_INDEXED_SINGLE_TRNS_SEMI_MASK,
    GD_PIXELFORMAT_REQCOLORS_ZERO_CLAMP_PAL8,
    GD_PIXELFORMAT_REQCOLORS_LARGE_CLAMP_PAL8,
    GD_PIXELFORMAT_PAL256_REQ255_FALLBACK_RGB,
    GD_PIXELFORMAT_PAL256_REQ256_KEEP_PAL8,
    GD_PIXELFORMAT_INDEXED_SINGLE_TRNS_SEMI_BG_FLOAT32_LINEAR,
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
                1,
                SIXEL_COLORSPACE_GAMMA,
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
                0,
                SIXEL_COLORSPACE_LINEAR,
                1
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
                0,
                SIXEL_COLORSPACE_GAMMA,
                1
            },
            { 0, 1, 256, NULL },
            new_gd_component
        },
        {
            "gd opaque truecolor with bg keeps rgb888 gamma path",
            "/tests/data/inputs/formats/snake-jpeg-444.jpg",
            {
                SIXEL_PIXELFORMAT_RGB888,
                64,
                64,
                1,
                -1,
                FRAME_METADATA_ANY,
                0,
                0,
                SIXEL_COLORSPACE_GAMMA,
                1
            },
            { 0, 0, 256, white_bg },
            new_gd_component
        },
        {
            "gd opaque indexed with bg stays rgb888 gamma",
            "/tests/data/inputs/formats/snake-png-pal8.png",
            {
                SIXEL_PIXELFORMAT_RGB888,
                64,
                64,
                1,
                -1,
                FRAME_METADATA_ANY,
                0,
                0,
                SIXEL_COLORSPACE_GAMMA,
                1
            },
            { 0, 1, 256, white_bg },
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
                0,
                SIXEL_COLORSPACE_GAMMA,
                1
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
                1,
                SIXEL_COLORSPACE_GAMMA,
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
                0,
                SIXEL_COLORSPACE_LINEAR,
                1
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
                0,
                SIXEL_COLORSPACE_LINEAR,
                1
            },
            { 1, 0, 256, NULL },
            new_gd_component
        },
        {
            "gd high-depth alpha png emits float32 plus mask",
            "/tests/data/inputs/formats/snake-png-rgba16-alpha.png",
            {
                SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
                2,
                1,
                1,
                -1,
                FRAME_METADATA_ANY,
                1,
                1,
                SIXEL_COLORSPACE_LINEAR,
                1
            },
            { 1, 0, 256, NULL },
            new_gd_component
        },
        {
            "gd indexed multi-trns emits rgb+mask fallback",
            "/tests/data/inputs/formats/libpng-pal8-trns-multi0-semi-icc.png",
            {
                SIXEL_PIXELFORMAT_RGB888,
                6,
                1,
                1,
                -1,
                FRAME_METADATA_ANY,
                1,
                1,
                SIXEL_COLORSPACE_GAMMA,
                1
            },
            { 1, 1, 256, NULL },
            new_gd_component
        },
        {
            "gd indexed reqcolors boundary keeps pal8+transparent index",
            "/tests/data/inputs/formats/pal8-trns-key0.png",
            {
                SIXEL_PIXELFORMAT_PAL8,
                4,
                1,
                1,
                FRAME_TRANSPARENT_NONNEG,
                FRAME_METADATA_ANY,
                0,
                0,
                SIXEL_COLORSPACE_GAMMA,
                1
            },
            { 1, 1, 4, NULL },
            new_gd_component
        },
        {
            "gd opaque truecolor keeps rgb888 gamma fast path",
            "/tests/data/inputs/formats/snake-jpeg-444.jpg",
            {
                SIXEL_PIXELFORMAT_RGB888,
                64,
                64,
                1,
                -1,
                FRAME_METADATA_ANY,
                0,
                0,
                SIXEL_COLORSPACE_GAMMA,
                1
            },
            { 0, 0, 256, NULL },
            new_gd_component
        },
        {
            "gd indexed input without palette request emits rgb888 gamma",
            "/tests/data/inputs/formats/snake-png-pal8.png",
            {
                SIXEL_PIXELFORMAT_RGB888,
                64,
                64,
                1,
                -1,
                FRAME_METADATA_ANY,
                0,
                0,
                SIXEL_COLORSPACE_GAMMA,
                1
            },
            { 0, 0, 256, NULL },
            new_gd_component
        },
        {
            "gd indexed keycolor gama+iccp with background emits float32",
            "/tests/data/inputs/formats/pal8-trns-key0-gama-icc.png",
            {
                SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
                4,
                1,
                1,
                -1,
                FRAME_METADATA_ANY,
                0,
                0,
                SIXEL_COLORSPACE_LINEAR,
                1
            },
            { 1, 1, 256, white_bg },
            new_gd_component
        },
        {
            "gd indexed keycolor gAMA-only with background emits float32",
            "/tests/data/inputs/formats/pal8-trns-key0-gama-only.png",
            {
                SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
                2,
                1,
                1,
                -1,
                FRAME_METADATA_ANY,
                0,
                0,
                SIXEL_COLORSPACE_LINEAR,
                1
            },
            { 1, 1, 256, white_bg },
            new_gd_component
        },
        {
            "gd rgb16 sRGB-only png promotes to float32 linear",
            "/tests/data/inputs/formats/snake_64_rgb16_srgb_only.png",
            {
                SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
                64,
                64,
                1,
                -1,
                FRAME_METADATA_ANY,
                0,
                0,
                SIXEL_COLORSPACE_LINEAR,
                1
            },
            { 1, 0, 256, NULL },
            new_gd_component
        },
        {
            "gd indexed single trns+semi emits rgb+mask",
            "/tests/data/inputs/formats/libpng-pal8-trns-single0-semi-icc.png",
            {
                SIXEL_PIXELFORMAT_RGB888,
                6,
                1,
                1,
                -1,
                FRAME_METADATA_ANY,
                1,
                1,
                SIXEL_COLORSPACE_GAMMA,
                1
            },
            { 1, 1, 256, NULL },
            new_gd_component
        },
        {
            "gd reqcolors zero clamps to pal8-safe default",
            "/tests/data/inputs/formats/pal8-trns-key0.png",
            {
                SIXEL_PIXELFORMAT_PAL8,
                4,
                1,
                1,
                FRAME_TRANSPARENT_NONNEG,
                FRAME_METADATA_ANY,
                0,
                0,
                SIXEL_COLORSPACE_GAMMA,
                1
            },
            { 1, 1, 0, NULL },
            new_gd_component
        },
        {
            "gd reqcolors above cap clamps to pal8-safe default",
            "/tests/data/inputs/formats/pal8-trns-key0.png",
            {
                SIXEL_PIXELFORMAT_PAL8,
                4,
                1,
                1,
                FRAME_TRANSPARENT_NONNEG,
                FRAME_METADATA_ANY,
                0,
                0,
                SIXEL_COLORSPACE_GAMMA,
                1
            },
            { 1, 1, SIXEL_PALETTE_MAX + 1, NULL },
            new_gd_component
        },
        {
            "gd pal256 reqcolors 255 falls back to rgb888",
            "/tests/data/inputs/formats/pal8-256colors.png",
            {
                SIXEL_PIXELFORMAT_RGB888,
                16,
                16,
                1,
                -1,
                FRAME_METADATA_ANY,
                0,
                0,
                SIXEL_COLORSPACE_GAMMA,
                1
            },
            { 1, 1, 255, NULL },
            new_gd_component
        },
        {
            "gd pal256 reqcolors 256 keeps pal8",
            "/tests/data/inputs/formats/pal8-256colors.png",
            {
                SIXEL_PIXELFORMAT_PAL8,
                16,
                16,
                1,
                -1,
                FRAME_METADATA_ANY,
                0,
                0,
                SIXEL_COLORSPACE_GAMMA,
                1
            },
            { 1, 1, 256, NULL },
            new_gd_component
        },
        {
            "gd indexed single trns+semi with background emits float32",
            "/tests/data/inputs/formats/libpng-pal8-trns-single0-semi-icc.png",
            {
                SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
                6,
                1,
                1,
                -1,
                FRAME_METADATA_ANY,
                0,
                0,
                SIXEL_COLORSPACE_LINEAR,
                1
            },
            { 1, 1, 256, white_bg },
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
    if (strcmp(mode, "opaque_rgb_with_bg_stays_rgb888_gamma") == 0) {
        return run_gd_pixelformat_case_by_id(
            GD_PIXELFORMAT_OPAQUE_RGB_WITH_BG_STAYS_RGB888_GAMMA);
    }
    if (strcmp(mode, "indexed_opaque_with_bg_stays_rgb888_gamma") == 0) {
        return run_gd_pixelformat_case_by_id(
            GD_PIXELFORMAT_INDEXED_OPAQUE_WITH_BG_STAYS_RGB888_GAMMA);
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
    if (strcmp(mode, "highdepth_alpha_no_bg_float32_mask") == 0) {
        return run_gd_pixelformat_case_by_id(
            GD_PIXELFORMAT_HIGHDEPTH_ALPHA_NO_BG_FLOAT32_MASK);
    }
    if (strcmp(mode, "indexed_multi_trns_mask") == 0) {
        return run_gd_pixelformat_case_by_id(
            GD_PIXELFORMAT_INDEXED_MULTI_TRNS_MASK);
    }
    if (strcmp(mode, "indexed_keycolor_reqcolors_boundary_pal8") == 0) {
        return run_gd_pixelformat_case_by_id(
            GD_PIXELFORMAT_INDEXED_KEYCOLOR_REQCOLORS_BOUNDARY_PAL8);
    }
    if (strcmp(mode, "opaque_rgb_gamma_fastpath") == 0) {
        return run_gd_pixelformat_case_by_id(
            GD_PIXELFORMAT_OPAQUE_RGB_GAMMA_FASTPATH);
    }
    if (strcmp(mode, "indexed_no_palette_rgb_gamma") == 0) {
        return run_gd_pixelformat_case_by_id(
            GD_PIXELFORMAT_INDEXED_NO_PALETTE_RGB_GAMMA);
    }
    if (strcmp(mode, "indexed_keycolor_gama_iccp_bg_float32_linear") == 0) {
        return run_gd_pixelformat_case_by_id(
            GD_PIXELFORMAT_INDEXED_KEYCOLOR_GAMA_ICCP_BG_FLOAT32_LINEAR);
    }
    if (strcmp(mode, "gama_only_bg_float32_linear") == 0) {
        return run_gd_pixelformat_case_by_id(
            GD_PIXELFORMAT_GAMA_ONLY_BG_FLOAT32_LINEAR);
    }
    if (strcmp(mode, "highdepth_srgb_only_float32_linear") == 0) {
        return run_gd_pixelformat_case_by_id(
            GD_PIXELFORMAT_HIGHDEPTH_SRGB_ONLY_FLOAT32_LINEAR);
    }
    if (strcmp(mode, "indexed_single_trns_semi_mask") == 0) {
        return run_gd_pixelformat_case_by_id(
            GD_PIXELFORMAT_INDEXED_SINGLE_TRNS_SEMI_MASK);
    }
    if (strcmp(mode, "reqcolors_zero_clamp_pal8") == 0) {
        return run_gd_pixelformat_case_by_id(
            GD_PIXELFORMAT_REQCOLORS_ZERO_CLAMP_PAL8);
    }
    if (strcmp(mode, "reqcolors_large_clamp_pal8") == 0) {
        return run_gd_pixelformat_case_by_id(
            GD_PIXELFORMAT_REQCOLORS_LARGE_CLAMP_PAL8);
    }
    if (strcmp(mode, "pal256_req255_fallback_rgb") == 0) {
        return run_gd_pixelformat_case_by_id(
            GD_PIXELFORMAT_PAL256_REQ255_FALLBACK_RGB);
    }
    if (strcmp(mode, "pal256_req256_keep_pal8") == 0) {
        return run_gd_pixelformat_case_by_id(
            GD_PIXELFORMAT_PAL256_REQ256_KEEP_PAL8);
    }
    if (strcmp(mode, "indexed_single_trns_semi_bg_float32_linear") == 0) {
        return run_gd_pixelformat_case_by_id(
            GD_PIXELFORMAT_INDEXED_SINGLE_TRNS_SEMI_BG_FLOAT32_LINEAR);
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
