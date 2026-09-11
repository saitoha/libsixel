/* Fix PSD raster-mask reconstruction with a complete RGB digest. */

#include "loader_builtin_memory_test_common.h"

int
test_loader_0160_loader_builtin_psd_mask_digest(int argc, char **argv)
{
    static uint64_t const expected[1] = {
        UINT64_C(0xac7f400a70bbabc9)
    };

    (void)argc;
    (void)argv;
    return edge_expect_fixture_rgb_digests(
        "PSD raster-mask reconstruction",
        "/tests/data/inputs/formats/"
        "snake16_rgb8_missing_composite_multilayer_mask.psd",
        1,
        16,
        16,
        1,
        expected);
}
