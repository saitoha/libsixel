/* SPDX-License-Identifier: MIT */
/* Verify curve1 error32 period. */
int test_refresh_adaptive_cadence(unsigned, unsigned, unsigned, unsigned);

int
test_delta_refresh_0062(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_refresh_adaptive_cadence(1, 32, 4, 10);
}
