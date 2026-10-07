/* SPDX-License-Identifier: MIT */
/* One encoder switches P3 intent and alpha interpretation. */
int test_highcolor_packed(int);

int
test_highcolor_0079(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_packed(79);
}
