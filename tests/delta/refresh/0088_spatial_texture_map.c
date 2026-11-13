/* SPDX-License-Identifier: MIT */
int test_refresh_spatial_map(int);

int
test_delta_refresh_0088(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_refresh_spatial_map(1);
}
