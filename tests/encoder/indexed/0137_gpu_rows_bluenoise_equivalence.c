/* SPDX-License-Identifier: MIT */
/* Row grouping preserves global blue-noise coordinates.
 * Policy: docs/threading/encoder.md
 */
#include "tests/_support/processing/gpu-palette/gpu_rows_common.h"

int
test_gpu_rows_bluenoise_equivalence(int argc, char **argv)
{
    gpu_rows_fixture_t f;
    int outcome;
    unsigned char expected[GPU_ROWS_PIXELS];
    int i;

    (void)argc;
    (void)argv;
    outcome = gpu_rows_open(&f, 1);
    if (outcome != 0) {
        gpu_rows_close(&f);
        return outcome;
    }
    outcome = 1;
    for (i = 0; i < GPU_ROWS_PIXELS * 3; ++i) {
        f.pixels[i] = (unsigned char)((i * 31 + i / 17) % 251);
    }
    f.request.method_for_diffuse = SIXEL_DIFFUSE_BLUENOISE_DITHER;
    f.request.bluenoise_size_override = 1;
    f.request.bluenoise_size = 64;
    f.request.bluenoise_strength_override = 1;
    f.request.bluenoise_strength = 16;
    f.request.bluenoise_phase_override = 1;
    f.request.bluenoise_phase_x = 19;
    f.request.bluenoise_phase_y = -7;
    f.request.rows_per_chunk = 0;
    ROWS_CHECK(gpu_rows_run(&f) == SIXEL_OK);
    memcpy(expected, f.output, sizeof(expected));
    f.request.rows_per_chunk = 6;
    ROWS_CHECK(gpu_rows_run(&f) == SIXEL_OK);
    ROWS_CHECK(f.calls == 8 && !memcmp(expected, f.output, sizeof(expected)));
    outcome = 0;
end:
    gpu_rows_close(&f);
    return outcome;
}
