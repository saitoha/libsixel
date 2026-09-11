/* Fix VP8 output for a stream using four token partitions. */

#include "loader_builtin_memory_test_common.h"

int
test_loader_0145_webp_vp8_token_partitions_digest(
    int argc,
    char **argv)
{
    uint64_t const expected[1] = { UINT64_C(0x9caad3d5667d27e1) };

    (void)argc;
    (void)argv;
    return edge_expect_fixture_rgb_digests(
        "WebP VP8 four token partitions",
        "/tests/data/inputs/vp80-04-partitions-1404.webp",
        1,
        176,
        144,
        1,
        expected);
}
