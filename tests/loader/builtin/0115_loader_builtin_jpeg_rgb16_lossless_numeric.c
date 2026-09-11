/* Fix high-precision output samples from a 16-bit lossless JPEG. */

#include "loader_builtin_memory_test_common.h"

int
test_loader_0115_loader_builtin_jpeg_rgb16_lossless_numeric(
    int argc,
    char **argv)
{
    edge_loader_options_t options;
    size_t const sample_pixels[3] = { 0u, 2048u, 4095u };
    float const expected[9] = {
        0.368627459f, 0.305882365f, 0.200000003f,
        0.498039216f, 0.447058827f, 0.0117647061f,
        0.411764711f, 0.36470589f, 0.294117659f
    };

    (void)argc;
    (void)argv;
    edge_loader_options_init(&options);
    options.require_static = 1;
    return edge_expect_fixture_float_samples(
        "JPEG RGB16 lossless",
        "/tests/data/inputs/formats/snake-jpeg-16bit-lossless.jpg",
        &options,
        64,
        64,
        SIXEL_PIXELFORMAT_RGBFLOAT32,
        SIXEL_COLORSPACE_GAMMA,
        sample_pixels,
        expected,
        0.000001f);
}
