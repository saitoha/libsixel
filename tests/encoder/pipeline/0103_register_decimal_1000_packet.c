/* SPDX-License-Identifier: MIT */
/* Verify one exact palette register spelling across a packet edge. */
int test_cpu_register(unsigned int, int, char const *);

int
test_cpu_serialize_0103(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_cpu_register(1000, 1, "#1000@");
}
