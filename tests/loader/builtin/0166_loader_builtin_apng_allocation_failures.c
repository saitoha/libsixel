/* Verify allocation-failure cleanup through APNG frame composition. */

#include "loader_builtin_memory_test_common.h"

int
test_loader_0166_loader_builtin_apng_allocation_failures(int argc,
                                                          char **argv)
{
    edge_loader_options_t options;

    (void)argc;
    (void)argv;
    edge_loader_options_init(&options);
    options.loop_control = SIXEL_LOOP_DISABLE;
    return edge_expect_fixture_allocation_failures(
        "APNG animation",
        "/tests/data/inputs/formats/apng_8x8_dispose_background.png",
        &options,
        0);
}
