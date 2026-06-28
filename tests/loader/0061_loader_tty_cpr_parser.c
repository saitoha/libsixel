/*
 * Verify CPR response parsing used by terminal cursor-position queries.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>
#include "src/tty.h"

typedef struct tty_cpr_0061_success_case {
    char const *response;
    int row;
    int col;
} tty_cpr_0061_success_case_t;

static int
tty_cpr_0061_run_success_cases(void)
{
    static tty_cpr_0061_success_case_t const cases[] = {
        { "\033[12;34R", 12, 34 },
        { "noise\033[1;1Rtail", 1, 1 },
        { "\2337;9R", 7, 9 }
    };
    size_t index;
    size_t count;
    int row;
    int col;
    SIXELSTATUS status;

    index = 0u;
    count = sizeof(cases) / sizeof(cases[0]);
    row = 0;
    col = 0;
    status = SIXEL_FALSE;

    for (index = 0u; index < count; ++index) {
        row = 0;
        col = 0;
        status = sixel_tty_parse_cpr_response(&row,
                                              &col,
                                              cases[index].response,
                                              strlen(cases[index].response));
        if (SIXEL_FAILED(status)) {
            fprintf(stderr, "CPR response should be accepted: case=%zu\n",
                    index);
            return 1;
        }
        if (row != cases[index].row || col != cases[index].col) {
            fprintf(stderr,
                    "CPR response parsed unexpected position: case=%zu\n",
                    index);
            return 1;
        }
    }

    return 0;
}

static int
tty_cpr_0061_run_failure_cases(void)
{
    static char const *const cases[] = {
        "",
        "abc",
        "\033[12;R",
        "\033[0;1R",
        "\033[1;0R",
        "\033[999999999999999999;1R",
        "\033[1;2"
    };
    size_t index;
    size_t count;
    int row;
    int col;
    SIXELSTATUS status;

    index = 0u;
    count = sizeof(cases) / sizeof(cases[0]);
    row = 0;
    col = 0;
    status = SIXEL_FALSE;

    for (index = 0u; index < count; ++index) {
        row = 0;
        col = 0;
        status = sixel_tty_parse_cpr_response(&row,
                                              &col,
                                              cases[index],
                                              strlen(cases[index]));
        if (status != SIXEL_FALSE) {
            fprintf(stderr, "CPR response should be rejected: case=%zu\n",
                    index);
            return 1;
        }
    }

    status = sixel_tty_parse_cpr_response(NULL, &col, "\033[1;1R", 6u);
    if (status != SIXEL_BAD_ARGUMENT) {
        fprintf(stderr, "CPR parser should reject NULL row output\n");
        return 1;
    }

    return 0;
}

int
test_loader_0061_loader_tty_cpr_parser(int argc, char **argv)
{
    int status;

    (void)argc;
    (void)argv;

    status = tty_cpr_0061_run_success_cases();
    if (status != 0) {
        return EXIT_FAILURE;
    }

    status = tty_cpr_0061_run_failure_cases();
    if (status != 0) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
