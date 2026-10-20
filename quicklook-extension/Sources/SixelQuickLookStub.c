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

/* CPU dispatch uses the topic diagnostic hook, but these decoder-only
 * targets do not link loader-common.c. Preserve its topic selection and
 * output contract without pulling image-loader backends into the codec. */
SIXEL_INTERNAL_API void
sixel_trace_topic_message(char const *topic, char const *format, ...)
{
    va_list args;

    if (!sixel_diagnostics_trace_topic_is_enabled(topic)) {
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

/* Keep the standalone decoder's failure-injection hook self-contained. */
char const *
sixel_test_environment_decoder_parallel_failure(void)
{
    return sixel_compat_getenv(
        "_SIXEL_TEST_DECODER_PARALLEL_FAILURE");
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
