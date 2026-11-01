/* SPDX-License-Identifier: MIT */
/* Verify native DRCS mono reject. */
int drcs_native_contract(int scenario);

int
test_drcs_native_0008(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return drcs_native_contract(7);
}
