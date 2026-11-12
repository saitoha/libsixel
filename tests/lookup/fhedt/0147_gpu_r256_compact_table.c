/* SPDX-License-Identifier: MIT */
/* Verify gpu r256 refinement uses a byte table. */
#include <string.h>
#include "../../_support/processing/gpu-palette/ordered_state_common.h"

int
test_fh_gpu_r256_compact_table(int argc, char **argv)
{
    ordered_state_fixture_t fixture;
    sixel_gpu_palette_test_hooks_t hooks;

    (void)argc;
    (void)argv;
    ordered_state_init(&fixture, 2, 1);
    memset(&hooks, 0, sizeof(hooks));
    fixture.request.test_hooks = &hooks;
    fixture.request.lut_policy = SIXEL_LUT_POLICY_FHEDT;
    fixture.request.fhedt_configured = 1;
    fixture.request.fhedt_resolution = 256;
    fixture.request.fhedt_refine = 1;
    fixture.request.fhedt_use_dist2 = 1;
    fixture.request.fhedt_weights[0] = 1;
    fixture.request.fhedt_weights[1] = 1;
    fixture.request.fhedt_weights[2] = 1;
    if (sixel_gpu_palette_apply(&fixture.request) != SIXEL_OK ||
            !hooks.published) return 1;
    /* Byte compaction is specific to the legacy Metal representation;
     * the Vulkan provider owns a uint-word table and no legacy table hook. */
    if (hooks.kernel == 9) return 77;
    if (hooks.kernel != 7) return 1;
    return hooks.fhedt_table_entry_bytes == 1U ? 0 : 1;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
