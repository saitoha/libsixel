/* SPDX-License-Identifier: MIT */
/* Reject lossless zero predictor. */
#include "../jpeg_process_test_common.h"

int
test_loader_jp_0126(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return jpeg_process_test(7, 36);
}
