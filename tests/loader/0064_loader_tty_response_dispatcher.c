/*
 * Verify that one tty read can be dispatched to OSC11 and CPR consumers.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>
#include "src/tty.h"

static int
tty_dispatcher_0064_run_combined_case(void)
{
    static char const response[] =
        "noise\033]11;rgb:12/34/56\007tail\033[17;29R";
    SIXELSTATUS status;
    unsigned int dispatched;
    unsigned char bgcolor[3];
    int row;
    int col;

    status = SIXEL_FALSE;
    dispatched = 0u;
    bgcolor[0] = 0u;
    bgcolor[1] = 0u;
    bgcolor[2] = 0u;
    row = 0;
    col = 0;

    status = sixel_tty_dispatch_response(&dispatched,
                                         bgcolor,
                                         &row,
                                         &col,
                                         response,
                                         strlen(response));
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "combined tty responses were not dispatched\n");
        return 1;
    }
    if (dispatched != (SIXEL_TTY_DISPATCHED_OSC11 |
                       SIXEL_TTY_DISPATCHED_CPR)) {
        fprintf(stderr, "combined tty response mask was incomplete\n");
        return 1;
    }
    if (bgcolor[0] != 0x12u || bgcolor[1] != 0x34u ||
            bgcolor[2] != 0x56u || row != 17 || col != 29) {
        fprintf(stderr, "combined tty response values were incorrect\n");
        return 1;
    }

    return 0;
}

static int
tty_dispatcher_0064_run_selective_cases(void)
{
    static char const response[] =
        "\033]11;rgb:aa/bb/cc\033\\\033[3;4R";
    SIXELSTATUS status;
    unsigned int dispatched;
    unsigned char bgcolor[3];
    int row;
    int col;

    status = SIXEL_FALSE;
    dispatched = 0u;
    bgcolor[0] = 0u;
    bgcolor[1] = 0u;
    bgcolor[2] = 0u;
    row = 0;
    col = 0;

    status = sixel_tty_dispatch_response(&dispatched,
                                         NULL,
                                         &row,
                                         &col,
                                         response,
                                         strlen(response));
    if (SIXEL_FAILED(status) || dispatched != SIXEL_TTY_DISPATCHED_CPR ||
            row != 3 || col != 4) {
        fprintf(stderr, "CPR-only dispatch failed\n");
        return 1;
    }

    dispatched = 0u;
    status = sixel_tty_dispatch_response(&dispatched,
                                         bgcolor,
                                         NULL,
                                         NULL,
                                         response,
                                         strlen(response));
    if (SIXEL_FAILED(status) || dispatched != SIXEL_TTY_DISPATCHED_OSC11 ||
            bgcolor[0] != 0xaau || bgcolor[1] != 0xbbu ||
            bgcolor[2] != 0xccu) {
        fprintf(stderr, "OSC11-only dispatch failed\n");
        return 1;
    }

    return 0;
}

static int
tty_dispatcher_0064_run_failure_cases(void)
{
    SIXELSTATUS status;
    unsigned int dispatched;
    int row;

    status = SIXEL_FALSE;
    dispatched = 1u;
    row = 0;

    status = sixel_tty_dispatch_response(&dispatched,
                                         NULL,
                                         NULL,
                                         NULL,
                                         "plain input",
                                         11u);
    if (status != SIXEL_FALSE || dispatched != 0u) {
        fprintf(stderr, "unrelated input should not be dispatched\n");
        return 1;
    }

    status = sixel_tty_dispatch_response(&dispatched,
                                         NULL,
                                         &row,
                                         NULL,
                                         "\033[1;2R",
                                         6u);
    if (status != SIXEL_BAD_ARGUMENT) {
        fprintf(stderr, "incomplete CPR destinations should be rejected\n");
        return 1;
    }

    return 0;
}

int
test_loader_0064_loader_tty_response_dispatcher(int argc, char **argv)
{
    int status;

    (void)argc;
    (void)argv;

    status = tty_dispatcher_0064_run_combined_case();
    if (status != 0) {
        return EXIT_FAILURE;
    }
    status = tty_dispatcher_0064_run_selective_cases();
    if (status != 0) {
        return EXIT_FAILURE;
    }
    status = tty_dispatcher_0064_run_failure_cases();
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
