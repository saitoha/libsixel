/* SPDX-License-Identifier: MIT */
/* One native Heckbert observation. */
int test_lookup_palette(int argc, char **argv);

int
test_heckbert_native_0029(int argc, char **argv)
{
    char *args[] = { "heckbert", "byte", "1", "0", "0", "heckbert" };

    (void)argc;
    (void)argv;
    return test_lookup_palette(6, args);
}
