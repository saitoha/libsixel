/* SPDX-License-Identifier: MIT */
/* Retain managed map precision with automatic base precision. */
int test_map_precision(int, int, char const *, char const *,
                       char const *, char const *);

int
test_map_precision_0006(int argc, char **argv)
{
    return argc == 2 ? test_map_precision(0, 0,
        "auto", "kmeans:seed=1",
        "builtin:cms_engine=builtin:cms_target=linear:prefer_8bit=0!",
        argv[1]) : 1;
}
