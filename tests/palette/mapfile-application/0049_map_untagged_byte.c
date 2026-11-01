/* SPDX-License-Identifier: MIT */
/* Keep an untagged byte map on the byte path with a CMS engine enabled. */
int test_map_precision(int, int, char const *, char const *,
                       char const *, char const *);

int
test_map_precision_0011(int argc, char **argv)
{
    return argc == 2 ? test_map_precision(1, 0,
        "8bit", "kmeans:seed=1",
        "builtin:cms_engine=builtin:cms_target=linear:prefer_8bit=0!",
        argv[1]) : 1;
}
