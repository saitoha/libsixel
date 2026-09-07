/*
 * Verify that the OSC11/shared background color parser accepts the supported
 * syntax used by -B / SIXEL_BGCOLOR.
 * Policy: docs/loader/background-policy.md
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include <sixel.h>

#include "src/tty.h"

typedef struct colorspec_success_case {
    char const *text;
    unsigned char red;
    unsigned char green;
    unsigned char blue;
} colorspec_success_case_t;

static int
run_colorspec_success_cases(void)
{
    static colorspec_success_case_t const cases[] = {
        { "#123", 0x10u, 0x20u, 0x30u },
        { "#112233", 0x11u, 0x22u, 0x33u },
        { "rgb:f/0/8", 0xf0u, 0x00u, 0x80u },
        { "rgb:ff/00/aa", 0xffu, 0x00u, 0xaau },
        { "black", 0x00u, 0x00u, 0x00u }
    };
    size_t index;
    SIXELSTATUS status;
    unsigned char parsed[3];

    index = 0u;
    status = SIXEL_FALSE;
    parsed[0] = 0u;
    parsed[1] = 0u;
    parsed[2] = 0u;

    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        parsed[0] = 0u;
        parsed[1] = 0u;
        parsed[2] = 0u;
        status = sixel_tty_parse_colorspec(parsed, cases[index].text);
        if (SIXEL_FAILED(status)) {
            fprintf(stderr,
                    "colorspec should be accepted: %s (status=%d)\n",
                    cases[index].text,
                    status);
            return 1;
        }
        if (parsed[0] != cases[index].red ||
                parsed[1] != cases[index].green ||
                parsed[2] != cases[index].blue) {
            fprintf(stderr,
                    "colorspec parsed unexpected RGB: %s -> "
                    "(%u,%u,%u)\n",
                    cases[index].text,
                    (unsigned int)parsed[0],
                    (unsigned int)parsed[1],
                    (unsigned int)parsed[2]);
            return 1;
        }
    }

    return 0;
}

int
test_loader_0050_loader_osc11_colorspec_parser(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return run_colorspec_success_cases() == 0
        ? EXIT_SUCCESS
        : EXIT_FAILURE;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
