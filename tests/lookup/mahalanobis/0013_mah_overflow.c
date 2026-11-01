/* SPDX-License-Identifier: MIT */
/* One observation, using the shared native-palette fixture and oracle. */
int test_extended_lookup(int argc, char **argv);

int
test_mah_lookup_0013(int argc, char **argv)
{
    char *args[] = { "lookup", "overflow", "5", "0" };

    (void)argc;
    (void)argv;
    return test_extended_lookup(4, args);
}
