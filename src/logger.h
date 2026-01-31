/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 */

#ifndef LIBSIXEL_LOG_H
#define LIBSIXEL_LOG_H

#include <stdio.h>

#include <sixel.h>

#include "threading.h"

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
