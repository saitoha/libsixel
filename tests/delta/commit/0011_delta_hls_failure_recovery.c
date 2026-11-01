/* SPDX-License-Identifier: MIT */
/* HLS encode failure and successful reuse clear the call-scoped comparison
 * hint. */
#include <sixel.h>
int test_delta_hls_failure(void);

int
test_delta_native_0173(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_hls_failure();
}
