/* SPDX-License-Identifier: MIT */
/* Do not admit excluded multiframe source jobs through map precision. */
int test_map_precision(int, int, char const *, char const *,
                       char const *, char const *);

int
test_map_precision_0019(int argc, char **argv)
{
    return argc == 2 ? test_map_precision(6, 0,
        "auto", "kmeans:seed=1",
        "builtin:cms_engine=builtin:cms_target=linear:prefer_8bit=0!",
        argv[1]) : 1;
}
