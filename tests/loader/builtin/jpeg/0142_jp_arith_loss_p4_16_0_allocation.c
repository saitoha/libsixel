/* SPDX-License-Identifier: MIT */
/* Fail every allocation boundary without leaking or publishing a frame. */
#include "../loader_builtin_memory_test_common.h"

int
test_loader_jp_0142(int argc, char **argv)
{
    edge_loader_options_t options;
    (void)argc;
    (void)argv;
    edge_loader_options_init(&options);
    options.require_static = 1;
    options.set_prefer_float32 = 1;
    options.prefer_float32 = 1;
    return edge_expect_fixture_allocation_failures("JPEG process",
        "/tests/data/color-numeric/jpeg-process-arith_loss_p4_16_0.jpg",
        &options, 0);
}
