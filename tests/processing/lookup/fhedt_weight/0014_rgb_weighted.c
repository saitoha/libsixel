/* SPDX-License-Identifier: MIT */
/* One independent FHEDT metric observation. */
int test_fh_weight_common(int argc, char **argv);

int
test_fh_weight_0014(int argc, char **argv)
{
    char *args[] = { "weight", "2", "default", "grid", "1" };

    (void)argc;
    (void)argv;
    return test_fh_weight_common(5, args);
}
