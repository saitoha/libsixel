/* SPDX-License-Identifier: MIT */
/* One observation, using the shared native-palette fixture and oracle. */
int test_lookup_palette(int argc, char **argv);

int
test_mah_palette_0010(int argc, char **argv)
{
    char *args[] = { "lookup", "wire", "7", "4", "1" };

    (void)argc;
    (void)argv;
    return test_lookup_palette(5, args);
}
