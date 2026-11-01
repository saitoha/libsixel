/* SPDX-License-Identifier: MIT */
/* One observation, using the shared native-palette fixture and oracle. */
int test_extended_lookup(int argc, char **argv);

int
test_eyt_lookup_0024(int argc, char **argv)
{
    char *args[] = { "lookup", "oracle_large", "6", "3" };

    (void)argc;
    (void)argv;
    return test_extended_lookup(4, args);
}
