/* SPDX-License-Identifier: MIT */
/* Verify native DRCS srgb float center. */
int drcs_native_contract(int scenario);

int
test_drcs_native_0001(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return drcs_native_contract(0);
}
