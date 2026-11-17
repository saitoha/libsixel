/* SPDX-License-Identifier: MIT */
int test_refresh_horizontal_cadence(unsigned int, unsigned int);

int
test_delta_refresh_0107(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_refresh_horizontal_cadence(2, 1);
}
