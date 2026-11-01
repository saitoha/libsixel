/* SPDX-License-Identifier: MIT */
/* Preserve the gap nine body recorded at e21311523. */
int test_compose_stream(int, char const *);

int
test_compose_0045(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_compose_stream(0, "#0@!9?@$#1!5?@");
}
