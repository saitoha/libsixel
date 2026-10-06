/* SPDX-License-Identifier: MIT */
/* HLS candidate keep uses decoded HLS RGB rather than directly rounded RGB
 * percentages. */
#include <sixel.h>
int test_delta_hls_candidate(int);

int
test_delta_native_0163(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_hls_candidate(0);
}
