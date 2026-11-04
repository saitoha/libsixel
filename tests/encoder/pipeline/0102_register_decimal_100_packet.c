/* SPDX-License-Identifier: MIT */
/* Verify one exact palette register spelling across a packet edge. */
int test_cpu_register(unsigned int, int, char const *);

int
test_cpu_serialize_0102(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_cpu_register(100, 1, "#100@");
}
