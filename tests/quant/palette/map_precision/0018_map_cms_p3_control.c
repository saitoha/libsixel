/* SPDX-License-Identifier: MIT */
/* Keep existing Display P3 map output as a precision control. */
int test_map_precision(int, int, char const *, char const *,
                       char const *, char const *);

int
test_map_precision_0018(int argc, char **argv)
{
    return argc == 2 ? test_map_precision(5, 0,
        "8bit", "kmeans:seed=1",
        "builtin:cms_engine=builtin:cms_target=linear:prefer_8bit=0!",
        argv[1]) : 1;
}
