/*
 * Verify animation cursor hide control helpers.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#if HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <sixel.h>
#include "src/encoder.h"
#include "src/tty.h"

static int
run_enable_flag_cases(void)
{
    if (!sixel_tty_is_animation_hide_cursor_enabled("1")) {
        fprintf(stderr, "expected only \"1\" to enable cursor hide\n");
        return 1;
    }
    if (sixel_tty_is_animation_hide_cursor_enabled("01") ||
            sixel_tty_is_animation_hide_cursor_enabled("true") ||
            sixel_tty_is_animation_hide_cursor_enabled("") ||
            sixel_tty_is_animation_hide_cursor_enabled(NULL)) {
        fprintf(stderr, "unexpected cursor hide value accepted\n");
        return 1;
    }

    return 0;
}

static int
run_output_condition_cases(void)
{
    if (!sixel_encoder_should_hide_animation_cursor(1, 0, 1, "1")) {
        fprintf(stderr, "expected hide condition for tty animation output\n");
        return 1;
    }
    if (sixel_encoder_should_hide_animation_cursor(0, 0, 1, "1") ||
            sixel_encoder_should_hide_animation_cursor(1, 1, 1, "1") ||
            sixel_encoder_should_hide_animation_cursor(1, 0, 0, "1") ||
            sixel_encoder_should_hide_animation_cursor(1, 0, 1, "true")) {
        fprintf(stderr, "unexpected hide condition accepted\n");
        return 1;
    }

    return 0;
}

static int
run_cleanup_helper_cases(void)
{
    SIXELSTATUS status;

    status = SIXEL_FALSE;

    status = sixel_tty_restore_cursor(STDOUT_FILENO);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "restore helper should allow no-op restore\n");
        return 1;
    }
    status = sixel_tty_restore_cursor(STDOUT_FILENO);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "restore helper should be idempotent\n");
        return 1;
    }
    status = sixel_tty_begin_animation_input_guard();
    if (status != SIXEL_OK && status != SIXEL_FALSE) {
        fprintf(stderr, "unexpected animation input guard begin status\n");
        return 1;
    }
    status = sixel_tty_end_animation_input_guard();
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "animation input guard end should allow no-op\n");
        return 1;
    }
    status = sixel_tty_end_animation_input_guard();
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "animation input guard end should be idempotent\n");
        return 1;
    }

    status = sixel_tty_hide_cursor(-1);
    if (status != SIXEL_BAD_ARGUMENT) {
        fprintf(stderr, "hide helper must reject invalid fd\n");
        return 1;
    }
    status = sixel_tty_restore_cursor(-1);
    if (status != SIXEL_BAD_ARGUMENT) {
        fprintf(stderr, "restore helper must reject invalid fd\n");
        return 1;
    }
    status = sixel_tty_restore_animation_cursor_to_bottom(-1, 1);
    if (status != SIXEL_BAD_ARGUMENT) {
        fprintf(stderr, "bottom restore helper must reject invalid fd\n");
        return 1;
    }
    status = sixel_tty_restore_animation_cursor_to_bottom(STDOUT_FILENO, 0);
    if (status != SIXEL_BAD_ARGUMENT) {
        fprintf(stderr, "bottom restore helper must reject invalid height\n");
        return 1;
    }

    return 0;
}

int
test_loader_0055_loader_animation_hide_cursor_control(int argc, char **argv)
{
    int status;

    (void)argc;
    (void)argv;

    status = run_enable_flag_cases();
    if (status != 0) {
        return EXIT_FAILURE;
    }

    status = run_output_condition_cases();
    if (status != 0) {
        return EXIT_FAILURE;
    }

    status = run_cleanup_helper_cases();
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
