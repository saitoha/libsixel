/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <string.h>
#include <sixel.h>
#include "src/dither.h"
#include "src/gpu-palette.h"

static int
residual_gate_write(char *data, int size, void *priv)
{
    (void)data;
    (void)priv;
    return size;
}

/* These byte witnesses exercise actual CPU/Metal keep decisions, separately
 * from encoder mask construction. Chromatic witnesses distinguish 2:4:1 from
 * max and unweighted RMS. The 65-pixel case checks non-divisible cell edges. */
int
test_residual_gate(int cells, int blue)
{
    unsigned char source[195], acc[195], valid[65], palette[12];
    unsigned char const *mask;
    size_t size;
    sixel_dither_t *dither;
    sixel_output_t *output;
    int pass, i, width, expected, ok;

    dither = NULL;
    output = NULL;
    ok = 0;
    width = cells ? 65 : 3;
    memset(acc, 102, sizeof(acc));
    memset(valid, 1, sizeof(valid));
    memset(source, 108, sizeof(source));
    memset(palette, 0, sizeof(palette));
    if (cells) {
        memset(palette, 108, 3);
        memset(palette + 3, 255, 3);
    } else {
        memset(source, 102, 9);
        source[2] = 123;
        source[4] = 113;
        source[6] = 117;
        memcpy(palette, source, 9);
    }
    if (sixel_dither_new(&dither, 4, NULL) != SIXEL_OK ||
            sixel_output_new(&output, residual_gate_write, NULL, NULL)
                != SIXEL_OK) goto end;
    sixel_dither_set_palette(dither, palette);
    sixel_dither_set_transparent(dither, 3);
    dither->keycolor_reserved = 1;
    dither->force_palette = 1;
    dither->bluenoise_strength_override = 1;
    dither->bluenoise_strength = 0.0f;
    dither->a_dither_strength_override = dither->x_dither_strength_override = 1;
    dither->a_dither_strength = dither->x_dither_strength = 0.0f;
    sixel_dither_set_pipeline_accumulation_result_enabled(dither, 1);
    sixel_dither_set_lut_policy(dither, SIXEL_LUT_POLICY_NONE);
    sixel_dither_set_diffusion_type(dither,
        blue == 2 ? SIXEL_DIFFUSE_A_DITHER :
        blue == 3 ? SIXEL_DIFFUSE_X_DITHER :
        blue ? SIXEL_DIFFUSE_BLUENOISE_DITHER : SIXEL_DIFFUSE_NONE);
    for (pass = 0; pass < 2; ++pass) {
        dither->gpu_policy = pass ? SIXEL_GPU_POLICY_FORCE
                                 : SIXEL_GPU_POLICY_OFF;
        sixel_dither_set_pipeline_accumulation_buffer_hint(dither,
            acc, (size_t)width * 3U, valid, (size_t)width,
            width, 1, 0, 0, width, 1, 3, 1, 8, SIXEL_6DELTA_ERROR_SKIP);
        dither->pipeline_6delta_metric = SIXEL_DELTA_METRIC_RGB241;
        if (cells) {
            dither->pipeline_residual_mask.columns = 64;
            dither->pipeline_residual_mask.rows = 1;
            dither->pipeline_residual_mask.width = width;
            dither->pipeline_residual_mask.height = 1;
            dither->pipeline_residual_mask.threshold = 0;
            dither->pipeline_residual_mask.cells[1] = (uint32_t)1 << 31;
        }
        if (sixel_encode(source, width, 1, 3, dither, output) != SIXEL_OK) {
            goto end;
        }
        mask = sixel_dither_get_pipeline_accumulation_result_mask(dither,
            &size);
        if (mask == NULL || size != (size_t)width) goto end;
        for (i = 0; i < width; ++i) {
            expected = (cells ? i < 63 : i == 0) ? 3 : 0;
            if (mask[i] != expected) {
                fprintf(stderr, "gate cpu/gpu=%d pixel=%d mask=%d want=%d\n",
                    pass, i, mask[i], expected);
                goto end;
            }
        }
    }
    ok = 1;
end:
    sixel_output_unref(output);
    sixel_dither_unref(dither);
    return ok ? 0 : 1;
}
