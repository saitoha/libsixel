/* Fix RGB lossless JPEG samples across component restart resets. */

#include "loader_builtin_memory_test_common.h"
#include "jpeg_lossless_test_common.h"

int
test_loader_0165_loader_builtin_jpeg_rgb_restart_numeric(int argc,
                                                          char **argv)
{
    static size_t const pixels[3] = { 0u, 1u, 2u };
    static float const expected[9] = {
        0.392156869f, 0.549019635f, 0.70588237f,
        0.862745106f, 0.235294119f, 0.062745102f,
        0.250980407f, 0.784313738f, 0.501960814f
    };
    unsigned char jpeg[512];
    edge_loader_options_t options;
    size_t jpeg_size;

    (void)argc;
    (void)argv;
    edge_loader_options_init(&options);
    options.require_static = 1;
    jpeg_size = jpeg_lossless_build_rgb_restart(jpeg, sizeof(jpeg));
    if (jpeg_size == 0u) {
        return 1;
    }
    return edge_expect_buffer_float_samples(
        "JPEG RGB lossless restart",
        jpeg,
        jpeg_size,
        &options,
        3,
        1,
        SIXEL_PIXELFORMAT_RGBFLOAT32,
        SIXEL_COLORSPACE_GAMMA,
        pixels,
        expected,
        0.0000001f);
}
