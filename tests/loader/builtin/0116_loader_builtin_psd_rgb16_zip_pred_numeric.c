/* Fix high-precision output samples from PSD 16-bit ZIP prediction. */

#include "loader_builtin_memory_test_common.h"

int
test_loader_0116_loader_builtin_psd_rgb16_zip_pred_numeric(
    int argc,
    char **argv)
{
    edge_loader_options_t options;
    size_t const sample_pixels[3] = { 0u, 128u, 255u };
    float const expected[9] = {
        0.368627459f, 0.301960796f, 0.196078435f,
        0.403921574f, 0.356862754f, 0.0235294122f,
        0.41568628f, 0.368627459f, 0.294117659f
    };

    (void)argc;
    (void)argv;
    edge_loader_options_init(&options);
    options.require_static = 1;
    return edge_expect_fixture_float_samples(
        "PSD RGB16 ZIP prediction",
        "/tests/data/inputs/formats/snake16_mode7_rgb16_zip_pred.psd",
        &options,
        16,
        16,
        SIXEL_PIXELFORMAT_RGBFLOAT32,
        SIXEL_COLORSPACE_GAMMA,
        sample_pixels,
        expected,
        0.000001f);
}
