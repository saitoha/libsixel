/*
 * Verify OSC11 response extraction for BEL and ST terminated payloads.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "src/tty.h"

typedef struct osc11_success_case {
    char const *response;
    unsigned char red;
    unsigned char green;
    unsigned char blue;
} osc11_success_case_t;

typedef struct osc11_failure_case {
    char const *response;
} osc11_failure_case_t;

static int
run_success_cases(void)
{
    static osc11_success_case_t const cases[] = {
        { "\033]11;#112233\007", 0x11u, 0x22u, 0x33u },
        { "noise\033]11;rgb:aa/bb/cc\033\\tail", 0xaau, 0xbbu, 0xccu }
    };
    size_t index;
    SIXELSTATUS status;
    unsigned char parsed[3];
    size_t response_size;

    index = 0u;
    status = SIXEL_FALSE;
    parsed[0] = 0u;
    parsed[1] = 0u;
    parsed[2] = 0u;
    response_size = 0u;

    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        response_size = strlen(cases[index].response);
        parsed[0] = 0u;
        parsed[1] = 0u;
        parsed[2] = 0u;
        status = sixel_tty_parse_osc11_response(parsed,
                                                cases[index].response,
                                                response_size);
        if (SIXEL_FAILED(status)) {
            fprintf(stderr,
                    "OSC11 response should be accepted: case=%zu\n",
                    index);
            return 1;
        }
        if (parsed[0] != cases[index].red ||
                parsed[1] != cases[index].green ||
                parsed[2] != cases[index].blue) {
            fprintf(stderr,
                    "OSC11 response parsed unexpected RGB: case=%zu -> "
                    "(%u,%u,%u)\n",
                    index,
                    (unsigned int)parsed[0],
                    (unsigned int)parsed[1],
                    (unsigned int)parsed[2]);
            return 1;
        }
    }

    return 0;
}

static int
run_failure_cases(void)
{
    static osc11_failure_case_t const cases[] = {
        { "#112233\007" },
        { "\033]11;not-a-color\007" },
        { "\033]11;#112233" },
        { "\033]10;#112233\007" }
    };
    size_t index;
    SIXELSTATUS status;
    unsigned char parsed[3];
    size_t response_size;

    index = 0u;
    status = SIXEL_FALSE;
    parsed[0] = 0u;
    parsed[1] = 0u;
    parsed[2] = 0u;
    response_size = 0u;

    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        response_size = strlen(cases[index].response);
        status = sixel_tty_parse_osc11_response(parsed,
                                                cases[index].response,
                                                response_size);
        if (SIXEL_SUCCEEDED(status)) {
            fprintf(stderr,
                    "OSC11 response should be rejected: case=%zu\n",
                    index);
            return 1;
        }
    }

    return 0;
}

int
test_loader_0051_loader_osc11_response_parser(int argc, char **argv)
{
    int status;

    (void)argc;
    (void)argv;

    status = run_success_cases();
    if (status != 0) {
        return EXIT_FAILURE;
    }

    status = run_failure_cases();
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
