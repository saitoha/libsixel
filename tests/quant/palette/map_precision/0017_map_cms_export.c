/* SPDX-License-Identifier: MIT */
/* Export sRGB8 from the native map while preserving working coordinates. */
int test_map_precision(int, int, char const *, char const *,
                       char const *, char const *);

int
test_map_precision_0017(int argc, char **argv)
{
    return argc == 2 ? test_map_precision(4, 0,
        "8bit", "kmeans:seed=1",
        "builtin:cms_engine=builtin:cms_target=linear:prefer_8bit=0!",
        argv[1]) : 1;
}
