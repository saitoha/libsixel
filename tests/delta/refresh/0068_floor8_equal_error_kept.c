/* SPDX-License-Identifier: MIT */
/* Verify floor8 equal error kept. */
int test_refresh_adaptive_cadence(unsigned, unsigned, unsigned, unsigned);

int
test_delta_refresh_0068(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_refresh_adaptive_cadence(1, 8, 8, 0);
}
