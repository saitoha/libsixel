/*
 * SPDX-License-Identifier: MIT
 *
 * Policy: docs/loader/background-policy.md
 *
 * Verify that the OSC11/shared background color parser rejects malformed and
 * unsupported color specifications.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include <sixel.h>

#include "src/tty.h"

int
test_loader_0065_loader_osc11_colorspec_reject(int argc, char **argv)
{
    static char const *const cases[] = {
        "",
        "rgb:1/2",
        "rgb:1/2/3/4",
        "#12",
        "rgb:zz/00/00",
        "no-such-color"
    };
    size_t index;
    SIXELSTATUS status;
    unsigned char parsed[3];

    (void)argc;
    (void)argv;
    index = 0u;
    status = SIXEL_FALSE;
    parsed[0] = 0u;
    parsed[1] = 0u;
    parsed[2] = 0u;

    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        status = sixel_tty_parse_colorspec(parsed, cases[index]);
        if (SIXEL_SUCCEEDED(status)) {
            fprintf(stderr,
                    "colorspec should be rejected: %s\n",
                    cases[index]);
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
