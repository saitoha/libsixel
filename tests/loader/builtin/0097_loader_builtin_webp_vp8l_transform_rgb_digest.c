/* Verify VP8L transform reversal with an exact decoded RGB digest. */

#include "loader_builtin_memory_test_common.h"

int
test_loader_0097_loader_builtin_webp_vp8l_transform_rgb_digest(
    int argc,
    char **argv)
{
    static uint64_t const expected_digests[1] = {
        UINT64_C(0x720d280b3b1ee1d6)
    };

    (void)argc;
    (void)argv;
    return edge_expect_fixture_rgb_digests(
        "WebP VP8L transform reversal",
        "/tests/data/inputs/formats/"
        "webp-lossless-rgb64-cwebp-transform-subsample.webp",
        1,
        64,
        64,
        1,
        expected_digests);
}
