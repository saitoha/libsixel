/*
 * SPDX-License-Identifier: MIT
 *
 * Policy: docs/loader/background-policy.md
 *
 * Verify that the OSC11 response parser rejects malformed, unterminated, and
 * unrelated replies.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "src/tty.h"

int
test_loader_0066_loader_osc11_response_reject(int argc, char **argv)
{
    static char const *const cases[] = {
        "#112233\007",
        "\033]11;not-a-color\007",
        "\033]11;#112233",
        "\033]10;#112233\007"
    };
    size_t index;
    size_t response_size;
    SIXELSTATUS status;
    unsigned char parsed[3];

    (void)argc;
    (void)argv;
    index = 0u;
    response_size = 0u;
    status = SIXEL_FALSE;
    parsed[0] = 0u;
    parsed[1] = 0u;
    parsed[2] = 0u;

    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        response_size = strlen(cases[index]);
        status = sixel_tty_parse_osc11_response(parsed,
                                                cases[index],
                                                response_size);
        if (SIXEL_SUCCEEDED(status)) {
            fprintf(stderr,
                    "OSC11 response should be rejected: case=%zu\n",
                    index);
            return EXIT_FAILURE;
        }
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
