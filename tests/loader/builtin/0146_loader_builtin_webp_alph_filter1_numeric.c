/* Fix the decoded RGB plane and transparency mask for ALPH filter 1. */

#include "loader_builtin_memory_test_common.h"

int
test_loader_0146_loader_builtin_webp_alph_filter1_numeric(int argc,
                                                           char **argv)
{
    edge_loader_options_t options;
    uint64_t const rgb[1] = { UINT64_C(0x7e2856775e2d89f2) };
    uint64_t const mask[1] = { UINT64_C(0x2ac06f32fd6a6b25) };

    (void)argc;
    (void)argv;
    edge_loader_options_init(&options);
    options.require_static = 1;
    return edge_expect_fixture_rgb_mask_digests(
        "WebP ALPH filter 1",
        "/tests/data/inputs/formats/webp-vp8-alpha-snake64-filter1.webp",
        &options,
        64,
        64,
        1,
        rgb,
        mask);
}
