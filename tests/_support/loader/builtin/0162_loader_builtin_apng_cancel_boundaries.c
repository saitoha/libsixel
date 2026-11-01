/* Fix cancellation checks before and between builtin APNG frames. */

#include "loader_builtin_memory_test_common.h"

int
test_loader_0162_loader_builtin_apng_cancel_boundaries(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return edge_expect_fixture_cancel_boundaries(
        "APNG cancellation",
        "/tests/data/inputs/formats/"
        "apng_8x8_libpng_delay_den_zero.png");
}
