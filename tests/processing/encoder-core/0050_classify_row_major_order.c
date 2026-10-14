/* SPDX-License-Identifier: MIT */
/* Verify classify row major order. */
int test_classify_stream(int, char const *);

int
test_classify_0050(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_classify_stream(0, "#1AA$#0C@");
}
