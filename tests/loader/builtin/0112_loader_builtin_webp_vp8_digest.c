/* Fix the builtin lossy VP8 decoder's RGB byte output. */

#include "loader_builtin_memory_test_common.h"

int
test_loader_0112_loader_builtin_webp_vp8_digest(int argc, char **argv)
{
    uint64_t const digest[1] = { UINT64_C(0x7e2856775e2d89f2) };

    (void)argc;
    (void)argv;
    return edge_expect_fixture_rgb_digests(
        "WebP VP8",
        "/tests/data/inputs/snake_64.webp",
        1,
        64,
        64,
        1,
        digest);
}
