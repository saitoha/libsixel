/* SPDX-License-Identifier: MIT */
/* Verify one exact palette register spelling. */
int test_cpu_register(unsigned int, int, char const *);

int
test_cpu_serialize_0100(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_cpu_register(1000, 0, "#1000@");
}
