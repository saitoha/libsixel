/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <sixel.h>
#include <stdarg.h>
#include <stdio.h>

#include "compat_stub.h"
#include "options.h"
#include "loader-common.h"

/* CPU dispatch and the common option parser use loader-common.c diagnostics.
 * Keep the same selection and output contracts in this standalone decoder
 * without importing the image-loader backends. */
int
sixel_trace_topic_is_enabled(char const *topic)
{
    return sixel_diagnostics_trace_topic_is_enabled(topic);
}

SIXEL_INTERNAL_API void
sixel_trace_topic_message(char const *topic, char const *format, ...)
{
    va_list args;

    if (!sixel_trace_topic_is_enabled(topic)) {
        return;
    }
    fprintf(stderr, "libsixel[%s]: ",
            topic != NULL && topic[0] != '\0' ? topic : "trace");
    va_start(args, format);
    sixel_compat_vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");
    fflush(stderr);
}

/* Path-option diagnostics use the loader topic and its unscoped prefix. */
int
loader_trace_is_enabled(void)
{
    return sixel_trace_topic_is_enabled("loader");
}

void
loader_trace_message(char const *format, ...)
{
    va_list args;

    if (!loader_trace_is_enabled()) {
        return;
    }
    fprintf(stderr, "libsixel: ");
    va_start(args, format);
    sixel_compat_vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
