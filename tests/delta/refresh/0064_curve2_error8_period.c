/* SPDX-License-Identifier: MIT */
/* Verify curve2 error8 period. */
int test_refresh_adaptive_cadence(unsigned, unsigned, unsigned, unsigned);

int
test_delta_refresh_0064(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_refresh_adaptive_cadence(2, 8, 4, 320);
}
