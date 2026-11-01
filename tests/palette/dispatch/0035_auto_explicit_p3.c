/* SPDX-License-Identifier: MIT */
/* Observe automatic palette selection: explicit p3. */
int test_auto_palette(int mode, int space);

int
test_auto_native_0021(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_auto_palette(16, 0);
}
