/* SPDX-License-Identifier: MIT */
/* Do not reuse a native static map in an excluded multiframe job. */
int test_map_precision(int, int, char const *, char const *,
                       char const *, char const *);

int
test_map_precision_0023(int argc, char **argv)
{
    return argc == 2 ? test_map_precision(8, 0,
        "8bit", "kmeans:seed=1",
        "builtin:cms_engine=builtin:cms_target=linear:prefer_8bit=0!",
        argv[1]) : 1;
}
