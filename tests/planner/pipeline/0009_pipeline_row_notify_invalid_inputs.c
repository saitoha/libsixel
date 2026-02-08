/*
 * SPDX-License-Identifier: MIT
 *
 * Direct API coverage for sixel_dither_pipeline_row_notify() edge inputs.
 *
 * This test validates two contracts that shell-level planner tests cannot
 * assert directly:
 *
 * +------------------------+-----------------------------+
 * | Input condition        | Expected observable effect  |
 * +------------------------+-----------------------------+
 * | row_index < 0          | logger emits job=-1         |
 * | pipeline_band_height=0 | logger emits job=-1         |
 * +------------------------+-----------------------------+
 *
 * Both cases must still invoke the installed callback with the original
 * row_index value so the producer and encoder remain synchronized.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/dither-common-pipeline.h"
#include "src/dither.h"
#include "src/logger.h"
#include "src/status.h"

typedef struct row_notify_spy {
    int called;
    int row_index;
} row_notify_spy_t;

static void
row_notify_callback(void *priv, int row_index)
{
    row_notify_spy_t *spy;

    spy = (row_notify_spy_t *)priv;
    if (spy == NULL) {
        return;
    }
    spy->called += 1;
    spy->row_index = row_index;
}

static int
file_contains(char const *path, char const *needle)
{
    FILE *file;
    char line[512];

    file = NULL;

    file = fopen(path, "r");
    if (file == NULL) {
        return 0;
    }
    while (fgets(line, sizeof(line), file) != NULL) {
        if (strstr(line, needle) != NULL) {
            fclose(file);
            return 1;
        }
    }
    fclose(file);
    return 0;
}

static int
run_case(char const *artifact_dir,
         char const *name,
         int band_height,
         int row_index,
         char const *expected_snippet)
{
    sixel_dither_t dither;
    sixel_logger_t logger;
    row_notify_spy_t spy;
    char log_path[1024];
    SIXELSTATUS status;

    status = SIXEL_FALSE;
    memset(&dither, 0, sizeof(dither));
    memset(&spy, 0, sizeof(spy));
    memset(log_path, 0, sizeof(log_path));

    if (artifact_dir == NULL || artifact_dir[0] == '\0') {
        fprintf(stderr, "%s: missing ARTIFACT_LOCAL_DIR\n", name);
        return 0;
    }

    if (snprintf(log_path,
                 sizeof(log_path),
                 "%s/%s.log",
                 artifact_dir,
                 name) >= (int)sizeof(log_path)) {
        fprintf(stderr, "%s: log path too long\n", name);
        return 0;
    }

    sixel_logger_init(&logger);
    status = sixel_logger_open(&logger, log_path);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: failed to open logger\n", name);
        return 0;
    }

    dither.pipeline_logger = &logger;
    dither.pipeline_band_height = band_height;
    dither.pipeline_row_callback = row_notify_callback;
    dither.pipeline_row_priv = &spy;

    sixel_dither_pipeline_row_notify(&dither, row_index);

    sixel_logger_close(&logger);

    if (spy.called != 1 || spy.row_index != row_index) {
        fprintf(stderr,
                "%s: callback mismatch (called=%d row=%d expected=%d)\n",
                name,
                spy.called,
                spy.row_index,
                row_index);
        return 0;
    }

    if (!file_contains(log_path, expected_snippet)) {
        fprintf(stderr,
                "%s: missing log snippet '%s' in %s\n",
                name,
                expected_snippet,
                log_path);
        return 0;
    }

    return 1;
}

int
test_pipeline_0009_pipeline_row_notify_invalid_inputs(int argc, char **argv)
{
    char const *artifact_dir;
    int success;

    (void)argc;
    (void)argv;

    success = 1;
    artifact_dir = getenv("ARTIFACT_LOCAL_DIR");

    if (!run_case(artifact_dir,
                  "row_negative",
                  6,
                  -1,
                  "\"event\":\"row_ready\",\"job\":-1")) {
        success = 0;
    }

    if (!run_case(artifact_dir,
                  "band_height_zero",
                  0,
                  12,
                  "\"event\":\"row_ready\",\"job\":-1")) {
        success = 0;
    }

    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
