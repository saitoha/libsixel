/* SPDX-License-Identifier: MIT */
/* Verify native DRCS or reject. */
int drcs_native_contract(int scenario);

int
test_drcs_native_0009(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return drcs_native_contract(8);
}
