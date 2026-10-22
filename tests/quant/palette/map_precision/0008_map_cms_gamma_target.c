/* SPDX-License-Identifier: MIT */
/* Preserve the map loader gamma-float representation before conversion. */
int test_map_precision(int, int, char const *, char const *,
                       char const *, char const *);

int
test_map_precision_0008(int argc, char **argv)
{
    return argc == 2 ? test_map_precision(0, 0,
        "auto", "kmeans:seed=1",
        "builtin:cms_engine=builtin:cms_target=gamma:prefer_8bit=0!",
        argv[1]) : 1;
}
