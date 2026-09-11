/* Fix representative CMYK16 ZIP-prediction conversion samples. */

#include "loader_builtin_memory_test_common.h"

int
test_loader_0152_loader_builtin_psd_cmyk16_zip_pred_numeric(int argc,
                                                             char **argv)
{
    edge_loader_options_t options;
    size_t const pixels[3] = { 0u, 128u, 255u };
    float const expected[9] = {
        0.0f, 0.0123160733f, 0.070613265f,
        0.0f, 0.00573032629f, 0.274924278f,
        0.0f, 0.00533220358f, 0.0244015604f
    };

    (void)argc;
    (void)argv;
    edge_loader_options_init(&options);
    options.require_static = 1;
    return edge_expect_fixture_float_samples(
        "PSD CMYK16 ZIP prediction",
        "/tests/data/inputs/formats/snake16_cmyk16_zip_pred.psd",
        &options,
        16,
        16,
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
        SIXEL_COLORSPACE_LINEAR,
        pixels,
        expected,
        0.0000001f);
}
