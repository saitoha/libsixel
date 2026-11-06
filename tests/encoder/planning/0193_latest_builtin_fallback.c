/* SPDX-License-Identifier: MIT */
int palette_feature_fallback(int);

int
test_palette_latest_builtin_fallback(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return palette_feature_fallback(1);
}
