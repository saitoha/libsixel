/* SPDX-License-Identifier: MIT */
/* Verify drcs full override. */
int test_delta_drcs_options(int);

int
test_delta_native_0217(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_drcs_options(3);
}
