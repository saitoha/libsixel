/* SPDX-License-Identifier: MIT */
/* Honor the explicit loader post-CMS byte narrowing boundary. */
int test_map_precision(int, int, char const *, char const *,
                       char const *, char const *);

int
test_map_precision_0010(int argc, char **argv)
{
    return argc == 2 ? test_map_precision(1, 0,
        "8bit", "kmeans:seed=1",
        "builtin:cms_engine=builtin:cms_target=gamma:prefer_8bit=1!",
        argv[1]) : 1;
}
