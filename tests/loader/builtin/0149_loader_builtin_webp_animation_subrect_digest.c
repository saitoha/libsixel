/* Fix complete canvases for lossy alpha animation subrect composition. */

#include "loader_builtin_memory_test_common.h"

int
test_loader_0149_loader_builtin_webp_animation_subrect_digest(int argc,
                                                               char **argv)
{
    uint64_t const expected[3] = {
        UINT64_C(0x9d0ec577972d46b2),
        UINT64_C(0x44d8a1ae2d409c97),
        UINT64_C(0xd57b346bae87fcfe)
    };

    (void)argc;
    (void)argv;
    return edge_expect_fixture_rgb_digests(
        "WebP alpha animation subrect",
        "/tests/data/inputs/formats/"
        "animated-lossy-alpha-subrect-80x64-3frame-min.webp",
        0,
        80,
        64,
        3,
        expected);
}
