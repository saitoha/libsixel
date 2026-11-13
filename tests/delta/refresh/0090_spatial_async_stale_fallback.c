/* SPDX-License-Identifier: MIT */
int test_refresh_spatial_async(int);

int
test_delta_refresh_0090(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_refresh_spatial_async(1);
}
