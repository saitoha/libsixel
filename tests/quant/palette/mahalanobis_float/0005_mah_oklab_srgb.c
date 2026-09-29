/* SPDX-License-Identifier: MIT */
/* One observation, using the shared native-palette fixture and oracle. */
int test_lookup_palette(int argc, char **argv);

int
test_mah_palette_0005(int argc, char **argv)
{
    char *args[] = { "lookup", "wire", "7", "2", "0" };

    (void)argc;
    (void)argv;
    return test_lookup_palette(5, args);
}
