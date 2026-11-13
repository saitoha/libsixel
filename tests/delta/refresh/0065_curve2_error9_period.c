/* SPDX-License-Identifier: MIT */
/* Verify curve2 error9 period. */
int test_refresh_adaptive_cadence(unsigned, unsigned, unsigned, unsigned);

int
test_delta_refresh_0065(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_refresh_adaptive_cadence(2, 9, 4, 80);
}
