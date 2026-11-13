/* SPDX-License-Identifier: MIT */
int test_refresh_spatial_cadence(int, unsigned int);

int
test_delta_refresh_0084(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_refresh_spatial_cadence(1, 1);
}
