/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <string.h>
#include <sixel.h>
#include "src/dither.h"
#include "src/gpu-palette.h"

/* One wrapper selects one policy/method and one observation. The offset
 * rectangle touches the bottom canvas edge and has padding on both sides.
 * Nonuniform rows and invalid pixels detect both RGB and mask stride errors. */
int
test_gpu_rectangle(int policy, int method, int observation)
{
    sixel_gpu_palette_request_t request;
    sixel_gpu_palette_test_hooks_t hooks;
    sixel_dither_t *cpu;
    sixel_index_t actual[35];
    sixel_index_t *expected;
    unsigned char source[35 * 3];
    unsigned char history[11 * 9 * 3];
    unsigned char valid[11 * 9];
    unsigned char palette[16 * 3];
    unsigned char skip[35];
    unsigned char const *cpu_skip;
    size_t size;
    int i, j, pos, ok;
    SIXELSTATUS status;

    cpu = NULL;
    expected = NULL;
    ok = 0;
    memset(&request, 0, sizeof(request));
    memset(&hooks, 0, sizeof(hooks));
    memset(skip, 0, sizeof(skip));
    for (i = 0; i < 48; ++i) palette[i] = (unsigned char)(i * 71 + 17);
    for (i = 0; i < 99; ++i) {
        valid[i] = i % 3 != 0;
        for (j = 0; j < 3; ++j)
            history[i * 3 + j] = (unsigned char)(i * 19 + j * 53);
    }
    for (i = 0; i < 35; ++i) {
        pos = (4 + i / 7) * 11 + 3 + i % 7;
        for (j = 0; j < 3; ++j)
            source[i * 3 + j] = i % 2 ? history[pos * 3 + j]
                : palette[(i % 16) * 3 + j];
    }
    request.policy = SIXEL_GPU_POLICY_FORCE;
    request.test_hooks = &hooks;
    request.dest = actual;
    request.pixels = source;
    request.pixel_count = 35;
    request.width = 7;
    request.height = 5;
    request.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    request.palette = palette;
    request.palette_size = sizeof(palette);
    request.palette_depth = 3;
    request.ncolors = 16;
    request.lut_policy = policy;
    if (policy == SIXEL_LUT_POLICY_FHEDT) {
        request.fhedt_configured = 1;
        request.fhedt_resolution = 64;
        request.fhedt_refine = 1;
        request.fhedt_weights[0] = 1;
        request.fhedt_weights[1] = 1;
        request.fhedt_weights[2] = 1;
    }
    request.method_for_diffuse = method;
    request.method_for_scan = SIXEL_SCAN_RASTER;
    request.sixdelta_enabled = request.has_6delta_accumulation = 1;
    request.accumulation_pixels = history;
    request.accumulation_pixels_size = sizeof(history);
    request.accumulation_width = 11;
    request.accumulation_height = 9;
    request.accumulation_origin_x = 3;
    request.accumulation_origin_y = 4;
    request.accumulation_valid_mask = valid;
    request.accumulation_valid_mask_size = sizeof(valid);
    request.accumulation_keycolor = 31;
    request.sixdelta_threshold = 8;
    request.accumulation_result_mask = skip;
    request.accumulation_result_mask_size = sizeof(skip);
    request.bluenoise_strength_override = 1;
    request.bluenoise_strength = 1.0f;
    request.bluenoise_phase_override = 1;
    request.bluenoise_phase_x = 48;
    request.bluenoise_phase_y = 52;
    request.positional_strength_override = 1;
    request.positional_strength = 1.0f;
    if (observation == 4) request.accumulation_origin_x = -1;
    if (observation == 5) request.accumulation_origin_y = 5;
    if (observation == 6) --request.accumulation_pixels_size;
    if (observation == 7) --request.accumulation_valid_mask_size;
    if (observation == 8) request.accumulation_width = 0;
    if (observation == 9) --request.pixel_count;
    if (observation == 10) request.accumulation_valid_mask = NULL;
    if (observation == 11) {
        request.policy = SIXEL_GPU_POLICY_AUTO;
        request.auto_threshold = 36;
    }
    if (observation == 12) {
        request.accumulation_width = 7;
        request.accumulation_origin_x = 0;
    }
    status = sixel_gpu_palette_apply(&request);
    if (observation == 11)
        return status == SIXEL_FALSE && !hooks.entered ? 0 : 1;
    if (observation >= 4 && observation <= 9) {
        /* Malformed views must fail before touching the backend. */
        return status == SIXEL_BAD_ARGUMENT && !hooks.entered ? 0 : 1;
    }
    if (status != SIXEL_OK || !hooks.entered || !hooks.published) goto end;
    if (policy == SIXEL_LUT_POLICY_CERTLUT && hooks.kernel != 9) goto end;
    if (observation == 2) {
        ok = hooks.retained_rgb_upload_bytes == sizeof(source);
        goto end;
    }
    if (observation == 3) {
        ok = hooks.retained_valid_upload_bytes == sizeof(skip);
        goto end;
    }
    if (SIXEL_FAILED(sixel_dither_new(&cpu, 16, NULL))) goto end;
    sixel_dither_set_palette(cpu, palette);
    sixel_dither_set_pixelformat(cpu, SIXEL_PIXELFORMAT_RGB888);
    sixel_dither_set_diffusion_type(cpu, method);
    sixel_dither_set_diffusion_scan(cpu, SIXEL_SCAN_RASTER);
    /* Byte Eytzinger intentionally uses a bounded approximate search. Match
     * that CPU policy and approximate FHEDT; exact policies use a scan. */
    sixel_dither_set_lut_policy(cpu, (policy == SIXEL_LUT_POLICY_EYTZINGER ||
        policy == SIXEL_LUT_POLICY_FHEDT) ? policy : SIXEL_LUT_POLICY_NONE);
    sixel_dither_set_optimize_palette(cpu, 0);
    sixel_dither_set_transparent(cpu, 31);
    cpu->gpu_policy = SIXEL_GPU_POLICY_OFF;
    cpu->lut_policy_fhedt_resolution_override = 1;
    cpu->lut_policy_fhedt_resolution = 64;
    cpu->lut_policy_fhedt_refine_override = 1;
    cpu->lut_policy_fhedt_refine = 1;
    cpu->lut_policy_fhedt_use_dist2_override = 1;
    cpu->lut_policy_fhedt_use_dist2 = 0;
    cpu->lut_policy_fhedt_use_cache_override = 1;
    cpu->lut_policy_fhedt_use_cache = 0;
    cpu->bluenoise_strength_override = 1;
    cpu->bluenoise_strength = 1.0f;
    cpu->bluenoise_phase_override = 1;
    cpu->bluenoise_phase_x = 48;
    cpu->bluenoise_phase_y = 52;
    cpu->a_dither_strength_override = cpu->x_dither_strength_override = 1;
    cpu->a_dither_strength = cpu->x_dither_strength = 1.0f;
    sixel_dither_set_pipeline_accumulation_buffer_hint(cpu, history,
        sizeof(history), request.accumulation_valid_mask, sizeof(valid),
        request.accumulation_width, request.accumulation_height,
        request.accumulation_origin_x, request.accumulation_origin_y,
        7, 5, 31, 1, 8, SIXEL_6DELTA_ERROR_DIFFUSE);
    sixel_dither_set_pipeline_accumulation_result_enabled(cpu, 1);
    expected = sixel_dither_apply_palette(cpu, source, 7, 5);
    if (expected == NULL) goto end;
    if (observation == 1) {
        cpu_skip = sixel_dither_get_pipeline_accumulation_result_mask(cpu,
            &size);
        ok = cpu_skip != NULL && size == sizeof(skip) &&
            memcmp(cpu_skip, skip, sizeof(skip)) == 0;
    } else {
        ok = memcmp(actual, expected, sizeof(actual)) == 0;
    }
end:
    if (!ok) fprintf(stderr, "GPU rectangle %d/%d/%d: %s\n", policy, method,
        observation, sixel_helper_get_additional_message());
    if (cpu != NULL) sixel_allocator_free(cpu->allocator, expected);
    sixel_dither_unref(cpu);
    return ok ? 0 : 1;
}
