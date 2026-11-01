/* SPDX-License-Identifier: MIT */
/* Verify native DRCS offset reject. */
int drcs_native_contract(int scenario);

int
test_drcs_native_0010(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return drcs_native_contract(9);
}
