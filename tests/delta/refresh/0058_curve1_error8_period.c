/* SPDX-License-Identifier: MIT */
/* Verify curve1 error8 period. */
int test_refresh_adaptive_cadence(unsigned, unsigned, unsigned, unsigned);

int
test_delta_refresh_0058(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_refresh_adaptive_cadence(1, 8, 4, 40);
}
