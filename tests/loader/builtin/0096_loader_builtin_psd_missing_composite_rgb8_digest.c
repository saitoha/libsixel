/* Verify PSD layer reconstruction with an exact decoded RGB digest. */

#include "loader_builtin_memory_test_common.h"

int
test_loader_0096_builtin_psd_missing_composite_rgb8_digest(
    int argc,
    char **argv)
{
    static uint64_t const expected_digests[1] = {
        UINT64_C(0xb842134ae920c08f)
    };

    (void)argc;
    (void)argv;
    return edge_expect_fixture_rgb_digests(
        "PSD missing-composite RGB8 layer reconstruction",
        "/tests/data/inputs/formats/"
        "snake16_rgb8_missing_composite_multilayer_normal.psd",
        1,
        16,
        16,
        1,
        expected_digests);
}
