/* SPDX-License-Identifier: MIT */
int test_refresh_spatial_option(char const *, int);

int
test_delta_refresh_0092(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_refresh_spatial_option("delta:T48:J1:L4:A1:G2", 1);
}
