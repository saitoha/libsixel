/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 */

#ifndef LIBSIXEL_PARALLEL_LOG_H
#define LIBSIXEL_PARALLEL_LOG_H

#include <stdio.h>

#include <sixel.h>

#include "sixel_threading.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sixel_parallel_logger {
    FILE *file;
    sixel_mutex_t mutex;
    int mutex_ready;
    int active;
    double started_at;
} sixel_parallel_logger_t;

void sixel_parallel_logger_init(sixel_parallel_logger_t *logger);
void sixel_parallel_logger_close(sixel_parallel_logger_t *logger);
SIXELSTATUS
sixel_parallel_logger_open(sixel_parallel_logger_t *logger, char const *path);
void sixel_parallel_logger_logf(sixel_parallel_logger_t *logger,
                                char const *role,
                                char const *worker,
                                char const *event,
                                int job_id,
                                int row_index,
                                int y0,
                                int y1,
                                int in0,
                                int in1,
                                char const *fmt,
                                ...);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_PARALLEL_LOG_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
