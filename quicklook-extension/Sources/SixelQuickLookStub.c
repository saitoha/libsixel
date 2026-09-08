/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <sixel.h>

#include "compat_stub.h"
#include "options.h"

/* Keep the standalone decoder's failure-injection hook self-contained. */
char const *
sixel_test_environment_decoder_paint_thread_create_failure(void)
{
    return sixel_compat_getenv(
        "_SIXEL_TEST_DECODER_PAINT_THREAD_CREATE_FAILURE");
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
