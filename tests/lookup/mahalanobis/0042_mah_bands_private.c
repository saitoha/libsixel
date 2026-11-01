/* SPDX-License-Identifier: MIT */
/* One observation, using the shared native-palette fixture and oracle. */
int test_lookup_palette(int argc, char **argv);

int
test_mah_palette_0012(int argc, char **argv)
{
    char *args[] = { "lookup", "bands", "7", "0", "0" };

    (void)argc;
    (void)argv;
    return test_lookup_palette(5, args);
}
