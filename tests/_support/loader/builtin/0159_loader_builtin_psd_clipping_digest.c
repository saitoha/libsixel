/* Fix PSD clipping reconstruction with a complete RGB digest. */

#include "loader_builtin_memory_test_common.h"

int
test_loader_0159_loader_builtin_psd_clipping_digest(int argc, char **argv)
{
    static uint64_t const expected[1] = {
        UINT64_C(0xb842134ae920c08f)
    };

    (void)argc;
    (void)argv;
    return edge_expect_fixture_rgb_digests(
        "PSD clipping reconstruction",
        "/tests/data/inputs/formats/"
        "snake16_rgb8_missing_composite_multilayer_clipping.psd",
        1,
        16,
        16,
        1,
        expected);
}
