/* SPDX-License-Identifier: MIT */
/* One observation, using the shared native-palette fixture and oracle. */
int test_extended_lookup(int argc, char **argv);

int
test_eyt_lookup_0008(int argc, char **argv)
{
    char *args[] = { "lookup", "fraction", "6", "2" };

    (void)argc;
    (void)argv;
    return test_extended_lookup(4, args);
}
