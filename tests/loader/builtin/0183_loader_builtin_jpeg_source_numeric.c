/* SPDX-License-Identifier: MIT */
/* Check unmanaged source values independently of the embedded ICC result. */
#include "src/cms.h"
#include "loader_builtin_memory_test_common.h"
#include "tests/data/color-numeric/jpeg.h"

int
test_loader_0183_jpeg_source_numeric(int argc, char **argv)
{
    edge_loader_options_t options;
    size_t const sample_pixels[3] = {0u, 2048u, 4095u};

    (void)argc;
    (void)argv;
    edge_loader_options_init(&options);
    options.require_static = 1;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;
    options.set_prefer_float32 = 1;
    options.prefer_float32 = 1;
    /* Independent libjpeg YCbCr samples and exact JFIF inverse equations.
     * The bound covers the decoder's rounded YCbCr constants and float. */
    return edge_expect_fixture_float_samples("JPEG source before ICC",
        "/tests/data/inputs/formats/snake-64-embedded-esrgb.jpg", &options,
        64, 64, SIXEL_PIXELFORMAT_RGBFLOAT32, SIXEL_COLORSPACE_GAMMA,
        sample_pixels, color_jpeg_raw, 0.0000005f);
}
