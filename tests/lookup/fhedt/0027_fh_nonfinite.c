/* SPDX-License-Identifier: MIT */
/* One observation using the shared FHEDT fixture. */
int test_fhedt_common(int argc, char **argv);

int
test_fh_lookup_0017(int argc, char **argv)
{
    char *args[] = { "lookup", "nonfinite", "0", "0", "0" };

    (void)argc;
    (void)argv;
    return test_fhedt_common(5, args);
}
