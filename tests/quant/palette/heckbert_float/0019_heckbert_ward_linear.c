/* SPDX-License-Identifier: MIT */
/* One native Heckbert observation. */
int test_heckbert_native(int argc, char **argv);

int
test_heckbert_native_0019(int argc, char **argv)
{
    char *args[] = { "heckbert", "10", "1" };

    (void)argc;
    (void)argv;
    return test_heckbert_native(3, args);
}
