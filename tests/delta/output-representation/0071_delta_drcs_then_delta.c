/* SPDX-License-Identifier: MIT */
/* Verify drcs then delta. */
int test_delta_drcs_options(int);

int
test_delta_native_0214(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_drcs_options(0);
}
