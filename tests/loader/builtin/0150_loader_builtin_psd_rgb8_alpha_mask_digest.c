/* Fix RGB and alpha-mask output for an uncomposited PSD alpha channel. */

#include "loader_builtin_memory_test_common.h"

int
test_loader_0150_loader_builtin_psd_rgb8_alpha_mask_digest(int argc,
                                                            char **argv)
{
    edge_loader_options_t options;
    uint64_t const rgb[1] = { UINT64_C(0x4642b19d2aea27e1) };
    uint64_t const mask[1] = { UINT64_C(0x4818e18da9c76589) };

    (void)argc;
    (void)argv;
    edge_loader_options_init(&options);
    options.require_static = 1;
    return edge_expect_fixture_rgb_mask_digests(
        "PSD RGB8 alpha mask",
        "/tests/data/inputs/formats/snake16_rgb8_alpha.psd",
        &options,
        16,
        16,
        1,
        rgb,
        mask);
}
