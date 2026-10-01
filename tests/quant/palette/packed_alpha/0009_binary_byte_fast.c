/* SPDX-License-Identifier: MIT */
/* Verify packed alpha: binary byte fast. */
int test_packed_unit(int first, int second);

int
test_packed_0009(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_packed_unit(0, 3);
}
