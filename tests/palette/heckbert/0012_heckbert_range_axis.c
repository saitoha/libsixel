/* SPDX-License-Identifier: MIT */
/* One native Heckbert observation. */
int test_heckbert_native(int argc, char **argv);

int
test_heckbert_native_0012(int argc, char **argv)
{
    char *args[] = { "heckbert", "7", "0" };

    (void)argc;
    (void)argv;
    return test_heckbert_native(3, args);
}
