/* Check JPEG ICC output against independently decoded source and ICC math. */

#include "src/cms.h"
#include "loader_builtin_memory_test_common.h"
#include "tests/data/color-numeric/jpeg.h"

int
test_loader_0110_loader_builtin_jpeg_icc_numeric(int argc, char **argv)
{
    edge_loader_options_t options;
    size_t const sample_pixels[3] = { 0u, 2048u, 4095u };

    (void)argc;
    (void)argv;
    edge_loader_options_init(&options);
    options.require_static = 1;
    options.cms_engine = SIXEL_CMS_ENGINE_BUILTIN;
    return edge_expect_fixture_float_samples(
        "JPEG embedded ICC",
        "/tests/data/inputs/formats/snake-64-embedded-esrgb.jpg",
        &options,
        64,
        64,
        SIXEL_PIXELFORMAT_RGBFLOAT32,
        SIXEL_COLORSPACE_GAMMA,
        sample_pixels,
        color_jpeg_srgb,
        0.000001f);
}
