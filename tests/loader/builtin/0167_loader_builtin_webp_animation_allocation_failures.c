/* Verify allocation failure cleanup through WebP animation composition. */

#include "loader_builtin_memory_test_common.h"

int
test_loader_0167_loader_builtin_webp_animation_alloc_failures(
    int argc,
    char **argv)
{
    edge_loader_options_t options;

    (void)argc;
    (void)argv;
    edge_loader_options_init(&options);
    options.loop_control = SIXEL_LOOP_DISABLE;
    return edge_expect_fixture_allocation_failures(
        "WebP animation",
        "/tests/data/inputs/formats/"
        "animated-lossy-alpha-subrect-80x64-3frame-min.webp",
        &options,
        0);
}
