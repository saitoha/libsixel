/* Fix alpha-bearing WebP animation background composition exactly. */

#include "loader_builtin_memory_test_common.h"

int
test_loader_0172_loader_builtin_webp_alpha_background_digest(
    int argc,
    char **argv)
{
    edge_loader_options_t options;
    uint64_t const expected_rgb[2] = {
        UINT64_C(0xb6a23e02628ad0e5),
        UINT64_C(0xfc633d8fbf4326e5)
    };

    (void)argc;
    (void)argv;
    edge_loader_options_init(&options);
    options.loop_control = SIXEL_LOOP_DISABLE;
    return edge_expect_fixture_digests_options(
        "WebP alpha animation background",
        "/tests/data/inputs/formats/"
        "animated-lossless-alpha-8x8-2frame-bg112233-a80.webp",
        &options,
        8,
        8,
        2,
        SIXEL_PIXELFORMAT_RGB888,
        SIXEL_COLORSPACE_GAMMA,
        expected_rgb);
}
