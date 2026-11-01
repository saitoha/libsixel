/* SPDX-License-Identifier: MIT */
/* Observe automatic palette selection: hard kmeans p3. */
int test_auto_palette(int mode, int space);

int
test_auto_native_0020(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_auto_palette(15, 0);
}
