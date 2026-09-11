/* Fix composited RGB output for a two-frame lossy WebP animation. */

#include "loader_builtin_memory_test_common.h"

int
test_loader_0113_loader_builtin_webp_lossy_animation_digest(
    int argc,
    char **argv)
{
    uint64_t const digest[2] = {
        UINT64_C(0x6eac67b7cbf60ce5),
        UINT64_C(0xf85aee9fda22c4e5)
    };

    (void)argc;
    (void)argv;
    return edge_expect_fixture_rgb_digests(
        "WebP lossy animation",
        "/tests/data/inputs/formats/animated-lossy-8x8-2frame-min.webp",
        0,
        8,
        8,
        2,
        digest);
}
