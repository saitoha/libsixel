/* SPDX-License-Identifier: MIT */
int test_refresh_spatial_defaults(int);

int
test_delta_refresh_0102(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_refresh_spatial_defaults(1);
}
