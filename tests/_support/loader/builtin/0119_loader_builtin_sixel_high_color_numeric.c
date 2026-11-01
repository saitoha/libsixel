/* Isolate final-palette collapse of register redefinition in the adapter. */

#include "loader_builtin_memory_test_common.h"

int
test_loader_0119_loader_builtin_sixel_high_color_numeric(
    int argc,
    char **argv)
{
    unsigned char const payload[] =
        "\033Pq\"1;1;2;1"
        "#0;2;100;0;0@"
        "#0;2;0;0;100@"
        "\033\\";
    unsigned char const expected[6] = {
        0u, 0u, 255u,
        0u, 0u, 255u
    };

    (void)argc;
    (void)argv;
    return edge_expect_rgb("SIXEL high-color final palette",
                           payload,
                           sizeof(payload) - 1u,
                           1,
                           2,
                           1,
                           1,
                           expected,
                           sizeof(expected));
}
