/* SPDX-License-Identifier: MIT */
/* Retain a managed map primary with byte source and linear work. */
int test_map_precision(int, int, char const *, char const *,
                       char const *, char const *);

int
test_map_precision_0002(int argc, char **argv)
{
    return argc == 2 ? test_map_precision(0, 1,
        "8bit", "kmeans:seed=1",
        "builtin:cms_engine=builtin:cms_target=linear:prefer_8bit=0!",
        argv[1]) : 1;
}
