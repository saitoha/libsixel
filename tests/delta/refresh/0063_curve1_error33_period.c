/* SPDX-License-Identifier: MIT */
/* Verify curve1 error33 period. */
int test_refresh_adaptive_cadence(unsigned, unsigned, unsigned, unsigned);

int
test_delta_refresh_0063(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_refresh_adaptive_cadence(1, 33, 4, 5);
}
