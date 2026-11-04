/* SPDX-License-Identifier: MIT */
/* Verify one exact palette register spelling. */
int test_cpu_register(unsigned int, int, char const *);

int
test_cpu_serialize_0101(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_cpu_register(65535, 0, "#65535@");
}
