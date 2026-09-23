/* Preserve all pixels after arithmetic recoding of progressive 4:2:0. */

#include "../loader_builtin_memory_test_common.h"

int
test_loader_jp_0162(int argc, char **argv)
{
    uint64_t const expected[1] = { UINT64_C(0xade587659436a76f) };

    (void)argc;
    (void)argv;
    return edge_expect_fixture_rgb_digests(
        "JPEG progressive 4:2:0",
        "/tests/data/color-numeric/jpeg-process-arith-420.jpg",
        1,
        64,
        64,
        1,
        expected);
}
