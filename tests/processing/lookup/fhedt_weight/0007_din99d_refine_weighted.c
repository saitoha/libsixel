/* SPDX-License-Identifier: MIT */
/* One independent FHEDT metric observation. */
int test_fh_weight_common(int argc, char **argv);

int
test_fh_weight_0007(int argc, char **argv)
{
    char *args[] = { "weight", "1", "4/1/1", "refine", "0" };

    (void)argc;
    (void)argv;
    return test_fh_weight_common(5, args);
}
