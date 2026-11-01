/* Check offset APNG blend over against linear expectations. */
int test_apng_0001_float(int argc, char **argv);

int
test_loader_0138_loader_builtin_apng_blend_over_numeric(
    int argc, char **argv)
{
    char *args[2] = {"apng", "16"};

    (void)argc;
    (void)argv;
    return test_apng_0001_float(2, args);
}
