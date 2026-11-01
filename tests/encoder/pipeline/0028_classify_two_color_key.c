/* SPDX-License-Identifier: MIT */
/* Key pixels in column words remain unpainted, without a color selector. */
int test_classify_stream(int, char const *);

int
test_classify_0056(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_classify_stream(3, "TiT");
}
