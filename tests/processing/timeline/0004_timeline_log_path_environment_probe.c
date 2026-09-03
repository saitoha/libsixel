/*
 * SPDX-License-Identifier: MIT
 *
 * Characterize the process-start timeline log path environment probe.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include <sixel.h>

#include "src/compat_stub.h"
#include "src/timeline-logger.h"

static int
timeline_log_path_environment_probe_is_one_shot(void)
{
    SIXELSTATUS status;
    sixel_timeline_logger_t *logger;
    int success;

    logger = NULL;
    success = 0;
    if (sixel_compat_setenv("SIXEL_LOG_PATH", "") != 0) {
        goto end;
    }
    status = sixel_timeline_logger_prepare_env(NULL, &logger);
    if (SIXEL_FAILED(status) || logger != NULL) {
        goto end;
    }
    if (sixel_compat_setenv("SIXEL_LOG_PATH", "late.jsonl") != 0) {
        goto end;
    }
    status = sixel_timeline_logger_prepare_env(NULL, &logger);
    if (SIXEL_FAILED(status) || logger != NULL) {
        goto end;
    }
    success = 1;

end:
    sixel_timeline_logger_unref(logger);
    (void)sixel_compat_setenv("SIXEL_LOG_PATH", "");
    return success;
}

int
test_timeline_0004_timeline_log_path_environment_probe(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (!timeline_log_path_environment_probe_is_one_shot()) {
        fprintf(stderr, "timeline log path environment probe changed\n");
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
