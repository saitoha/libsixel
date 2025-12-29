/*
 * Test harness for cli_option_requires_argument optstring parsing.
 */

#include <stdio.h>

#include "config.h"
#include "converters/cli.h"

typedef struct test_case {
    int short_opt;
    int expected;
} test_case_t;

static void
print_result(int index, int success, char const *message)
{
    printf("%s %d - %s\n",
           success ? "ok" : "not ok",
           index,
           message);
}

int
main(void)
{
    char const optstring[] = "a:b::c";
    test_case_t cases[] = {
        { 'a', 1 },
        { 'b', 1 },
        { 'c', 0 },
        { 'z', 0 },
    };
    size_t index;
    int status;
    int requires;

    printf("1..%zu\n", sizeof(cases) / sizeof(cases[0]));

    status = 0;
    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); index++) {
        requires = cli_option_requires_argument(optstring,
                                                cases[index].short_opt);
        if (requires == cases[index].expected) {
            print_result((int)(index + 1u), 1, "optstring parsed");
        } else {
            print_result((int)(index + 1u), 0, "optstring mismatch");
            status = 1;
        }
    }

    return status;
}
