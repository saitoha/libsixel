/* SPDX-License-Identifier: MIT */
/* Verify native DRCS mask recovery. */
int drcs_native_contract(int scenario);

int
test_drcs_native_0007(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return drcs_native_contract(6);
}
