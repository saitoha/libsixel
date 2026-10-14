/* SPDX-License-Identifier: MIT */
/* Converter-owned output intent and per-image terminal policy lifecycle. */
#ifndef IMG2SIXEL_TERMINAL_COLOR_JOB_H
#define IMG2SIXEL_TERMINAL_COLOR_JOB_H

#include <sixel.h>
#include "terminal-color-tty.h"

typedef struct img2sixel_color_job {
    int automatic;
    char output_suboptions[256];
    int timeout_ms;
    int timeout_override;
    int stdout_only;
    int drcs;
    int requested;
    int probed;
    int selected_policy;
    int output_fd;
    img2sixel_color_tty_t *tty;
    img2sixel_color_cancel_t cancel;
    void *cancel_context;
} img2sixel_color_job_t;

void img2sixel_color_job_init(img2sixel_color_job_t *job);

/* Explicit values continue through the existing library choice parser. */
SIXELSTATUS img2sixel_color_job_option(img2sixel_color_job_t *job,
                                       sixel_encoder_t *encoder,
                                       char const *argument);

/* Any non-'-' outfile disqualifies auto, including '-o file -o -'. */
void img2sixel_color_job_outfile(img2sixel_color_job_t *job,
                                char const *argument);

/* Resolve environment defaults after --env and all explicit CLI options. */
SIXELSTATUS img2sixel_color_job_prepare(img2sixel_color_job_t *job,
                                        sixel_encoder_t *encoder,
                                        int output_fd,
                                        img2sixel_color_cancel_t cancel,
                                        void *context);

/* No CLI reader or borrowed termios remains during synchronous encode. */
SIXELSTATUS img2sixel_color_job_encode(img2sixel_color_job_t *job,
                                       sixel_encoder_t *encoder,
                                       char const *input);

SIXELSTATUS img2sixel_color_job_close(img2sixel_color_job_t *job);

#endif /* IMG2SIXEL_TERMINAL_COLOR_JOB_H */
