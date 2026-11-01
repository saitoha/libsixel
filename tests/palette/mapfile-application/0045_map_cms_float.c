/* SPDX-License-Identifier: MIT */
/* Keep explicit float source and map behavior as a control. */
int test_map_precision(int, int, char const *, char const *,
                       char const *, char const *);

int
test_map_precision_0007(int argc, char **argv)
{
    return argc == 2 ? test_map_precision(0, 0,
        "float32", "kmeans:seed=1",
        "builtin:cms_engine=builtin:cms_target=linear:prefer_8bit=0!",
        argv[1]) : 1;
}
