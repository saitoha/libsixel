/* SPDX-License-Identifier: MIT */
/* One observation using the shared FHEDT fixture. */
int test_lookup_palette(int argc, char **argv);

int
test_fh_palette_0001(int argc, char **argv)
{
    char *args[] = { "lookup", "wire", "9", "0", "0" };

    (void)argc;
    (void)argv;
    return test_lookup_palette(5, args);
}
