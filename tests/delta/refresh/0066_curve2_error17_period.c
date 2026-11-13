/* SPDX-License-Identifier: MIT */
/* Verify curve2 error17 period. */
int test_refresh_adaptive_cadence(unsigned, unsigned, unsigned, unsigned);

int
test_delta_refresh_0066(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_refresh_adaptive_cadence(2, 17, 4, 20);
}
