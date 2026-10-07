/* SPDX-License-Identifier: MIT */
/* Gray-alpha honors the background through CIELAB and P3. */
int test_highcolor_packed(int);

int
test_highcolor_0070(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_packed(70);
}
