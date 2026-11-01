/* SPDX-License-Identifier: MIT */
/* One native Heckbert observation. */
int test_lookup_palette(int argc, char **argv);

int
test_heckbert_native_0025(int argc, char **argv)
{
    char *args[] = { "heckbert", "wire", "1", "2", "0", "heckbert" };

    (void)argc;
    (void)argv;
    return test_lookup_palette(6, args);
}
