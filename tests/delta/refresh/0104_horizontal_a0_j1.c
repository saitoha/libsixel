/* SPDX-License-Identifier: MIT */
int test_refresh_horizontal_cadence(unsigned int, unsigned int);

int
test_delta_refresh_0104(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_refresh_horizontal_cadence(1, 0);
}
