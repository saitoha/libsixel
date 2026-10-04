/* SPDX-License-Identifier: MIT */
/* Verify native DRCS macro reject. */
int drcs_native_contract(int scenario);

int
test_drcs_native_0011(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return drcs_native_contract(10);
}
