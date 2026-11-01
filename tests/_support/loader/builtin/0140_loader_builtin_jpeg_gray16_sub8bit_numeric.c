/* Prove that lossless JPEG samples below eight-bit increments survive. */

#include "loader_builtin_memory_test_common.h"

int
test_loader_0140_loader_builtin_jpeg_gray16_sub8bit_numeric(int argc,
                                                             char **argv)
{
    edge_loader_options_t options;
    size_t const pixels[3] = { 0u, 1u, 2u };
    float const expected[9] = {
        0.311589241f, 0.311589241f, 0.311589241f,
        0.303471416f, 0.303471416f, 0.303471416f,
        0.297856092f, 0.297856092f, 0.297856092f
    };

    (void)argc;
    (void)argv;
    edge_loader_options_init(&options);
    options.require_static = 1;
    return edge_expect_fixture_float_samples(
        "JPEG gray16 sub-eight-bit",
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
