/* Fix high-precision samples across lossless JPEG restart boundaries. */

#include "loader_builtin_memory_test_common.h"

int
test_loader_0142_loader_builtin_jpeg_gray16_restart_numeric(int argc,
                                                             char **argv)
{
    edge_loader_options_t options;
    size_t const pixels[3] = { 63u, 64u, 65u };
    float const expected[9] = {
        0.5544976f, 0.5544976f, 0.5544976f,
        0.308232248f, 0.308232248f, 0.308232248f,
        0.303181499f, 0.303181499f, 0.303181499f
    };

    (void)argc;
    (void)argv;
    edge_loader_options_init(&options);
    options.require_static = 1;
    return edge_expect_fixture_float_samples(
        "JPEG gray16 restart",
        "/tests/data/inputs/formats/"
        "snake-jpeg-16bit-lossless-gray-restart.jpg",
        &options,
        64,
        64,
        SIXEL_PIXELFORMAT_RGBFLOAT32,
        SIXEL_COLORSPACE_GAMMA,
        pixels,
        expected,
        0.0000001f);
}
