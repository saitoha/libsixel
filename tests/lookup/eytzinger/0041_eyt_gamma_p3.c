/* SPDX-License-Identifier: MIT */
/* One observation, using the shared native-palette fixture and oracle. */
int test_lookup_palette(int argc, char **argv);

int
test_eyt_palette_0002(int argc, char **argv)
{
    char *args[] = { "lookup", "wire", "8", "0", "1" };

    (void)argc;
    (void)argv;
    return test_lookup_palette(5, args);
}
