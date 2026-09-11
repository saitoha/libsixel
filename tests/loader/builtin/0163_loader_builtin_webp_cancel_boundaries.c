/* Fix cancellation checks before and between builtin WebP frames. */

#include "loader_builtin_memory_test_common.h"

int
test_loader_0163_loader_builtin_webp_cancel_boundaries(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return edge_expect_fixture_cancel_boundaries(
        "WebP animation cancellation",
        "/tests/data/inputs/formats/"
        "animated-lossy-8x8-2frame-min.webp");
}
