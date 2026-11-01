/* Fix representative Lab32 ZIP-prediction conversion samples. */

#include "loader_builtin_memory_test_common.h"

int
test_loader_0153_loader_builtin_psd_lab32_zip_pred_numeric(int argc,
                                                            char **argv)
{
    edge_loader_options_t options;
    size_t const pixels[3] = { 0u, 128u, 255u };
    float const expected[9] = {
        0.33811751f, 0.0600231066f, 0.373450994f,
        0.385148704f, -0.0803320035f, 0.883570313f,
        0.405372649f, 0.031129187f, 0.253592223f
    };

    (void)argc;
    (void)argv;
    edge_loader_options_init(&options);
    options.require_static = 1;
    return edge_expect_fixture_float_samples(
        "PSD Lab32 ZIP prediction",
        "/tests/data/inputs/formats/snake16_lab32_zip_pred.psd",
        &options,
        16,
        16,
        SIXEL_PIXELFORMAT_CIELABFLOAT32,
        SIXEL_COLORSPACE_CIELAB,
        pixels,
        expected,
        0.0000001f);
}
