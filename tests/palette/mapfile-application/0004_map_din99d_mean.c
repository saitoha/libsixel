/* SPDX-License-Identifier: MIT */
int test_map_cluster(int mode, char const *path);

int
test_map_cluster_0004(int argc, char **argv)
{
    return argc == 2 ? test_map_cluster(3, argv[1]) : 1;
}
