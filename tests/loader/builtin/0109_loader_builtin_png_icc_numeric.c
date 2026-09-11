/* Fix builtin-CMS float output for an embedded-profile PNG. */

#include "src/cms.h"
#include "loader_builtin_memory_test_common.h"

int
test_loader_0109_loader_builtin_png_icc_numeric(int argc, char **argv)
{
    edge_loader_options_t options;
    size_t const sample_pixels[3] = { 0u, 651u, 1301u };
    float const expected[9] = {
        0.319717675f, 0.023515204f, 0.022885453f,
        0.319717675f, 0.023515204f, 0.022885453f,
        0.0f, 0.0f, 0.0f
    };

    (void)argc;
    (void)argv;
    edge_loader_options_init(&options);
    options.require_static = 1;
    options.cms_engine = SIXEL_CMS_ENGINE_BUILTIN;
    return edge_expect_fixture_float_samples(
        "PNG embedded ICC",
        "/tests/data/inputs/formats/map8_embedded_icc.png",
        &options,
        93,
        14,
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
        SIXEL_COLORSPACE_LINEAR,
        sample_pixels,
        expected,
        0.000001f);
}
