/* SPDX-License-Identifier: MIT */
/* One independent FHEDT metric observation. */
int test_fh_weight_common(int argc, char **argv);

int
test_fh_weight_0019(int argc, char **argv)
{
    char *args[] = { "weight", "3", "auto", "refine", "0" };

    (void)argc;
    (void)argv;
    return test_fh_weight_common(5, args);
}
