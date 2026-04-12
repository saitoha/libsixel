/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef LIBSIXEL_LOG_H
#define LIBSIXEL_LOG_H

#include <stdio.h>

#include <sixel.h>

#include "threading.h"

#define SIXEL_LOGGER_FRAME_CONTEXT_SLOTS 64

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sixel_logger {
    /*
     * Delegated sink used when reusing an already opened logger instead of
     * reopening the file. This keeps mutex ownership with the first opener.
     */
    struct sixel_logger *delegate;
    FILE *file;
    sixel_mutex_t mutex;
    int mutex_ready;
    int active;
    double started_at;
    /*
     * Keep frame metadata per logging thread so animated pipeline workers can
     * stamp each event with frame/loop context without adding parameters to
     * every log call site.
     */
    struct {
        unsigned long long thread_id;
        int frame_no;
        int loop_no;
        int multiframe;
        int active;
    } frame_contexts[SIXEL_LOGGER_FRAME_CONTEXT_SLOTS];
} sixel_logger_t;

void sixel_logger_init(sixel_logger_t *logger);
void sixel_logger_close(sixel_logger_t *logger);
SIXELSTATUS
sixel_logger_open(sixel_logger_t *logger, char const *path);
SIXELSTATUS
sixel_logger_prepare_env(sixel_logger_t *logger);
void sixel_logger_logf(sixel_logger_t *logger,
                       char const *role,
                       char const *worker,
                       char const *event,
                       int job_id,
                       ...);
void sixel_logger_set_frame_context(sixel_logger_t *logger,
                                    int frame_no,
                                    int loop_no,
                                    int multiframe);
void sixel_logger_clear_frame_context(sixel_logger_t *logger);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_LOG_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
