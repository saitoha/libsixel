/* SPDX-License-Identifier: MIT */
int test_refresh_horizontal_configuration(int);

int
test_delta_refresh_0118(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_refresh_horizontal_configuration(4);
}
