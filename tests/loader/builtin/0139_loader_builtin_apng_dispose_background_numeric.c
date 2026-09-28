/* Check offset APNG dispose background against linear expectations. */
int test_apng_0001_float(int argc, char **argv);

int
test_loader_0139_loader_builtin_apng_dispose_background_numeric(
    int argc, char **argv)
{
    char *args[2] = {"apng", "17"};

    (void)argc;
    (void)argv;
    return test_apng_0001_float(2, args);
}
