/* SPDX-License-Identifier: MIT */
/* The legacy adapter retains whole-image dispatch.
 * Policy: docs/threading/encoder.md
 */
#include <string.h>

#include "tests/_support/processing/gpu-palette/gpu_rows_common.h"

int
test_gpu_rows_legacy_whole(int argc, char **argv)
{
    gpu_rows_fixture_t f;
    int outcome;

    (void)argc;
    (void)argv;
    outcome = gpu_rows_open(&f, 1);
    if (outcome != 0) {
        gpu_rows_close(&f);
        return outcome;
    }
    /* Only the Metal palette application keeps a program-less legacy
     * executor; another runtime refuses a request without a program. */
    if (strcmp(f.device->vtbl->program_format(f.device),
            "metal.lookup-rgb8.v1") != 0) {
        gpu_rows_close(&f);
        return 77;
    }
    outcome = 1;
    f.request.program = NULL;
    f.request.lut_policy = SIXEL_LUT_POLICY_NONE;
    ROWS_CHECK(gpu_rows_run(&f) == SIXEL_OK);
    ROWS_CHECK(f.hooks.submitted_chunks == 1 && f.calls == 0);
    outcome = 0;
end:
    gpu_rows_close(&f);
    return outcome;
}
