/* SPDX-License-Identifier: MIT */
/* Keep byte application when map CMS is explicitly disabled. */
int test_map_precision(int, int, char const *, char const *,
                       char const *, char const *);

int
test_map_precision_0009(int argc, char **argv)
{
    return argc == 2 ? test_map_precision(1, 0,
        "8bit", "kmeans:seed=1",
        "builtin:cms_engine=none!",
        argv[1]) : 1;
}
