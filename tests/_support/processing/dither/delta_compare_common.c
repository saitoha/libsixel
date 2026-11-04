/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include <6cells.h>
#include "src/dither.h"
#include "src/dither-policy-private.h"
#include "src/gpu-palette.h"
#include "src/lookup-policy-ordered-gpu.h"
#include "src/encoder.h"
#include "src/sixel_decode_pixels.h"
#include "src/compat_stub.h"

#define DC_CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "delta compare line %d: %s (%s)\n", __LINE__, #x, \
        sixel_helper_get_additional_message()); goto end; } } while (0)

typedef struct dc_lookup {
    sixel_lookup_policy_interface_t iface;
    int calls;
} dc_lookup_t;

static int
dc_map(sixel_lookup_policy_interface_t const *self, unsigned char const *p)
{
    dc_lookup_t *probe;

    (void)p;
    probe = (dc_lookup_t *)(void *)self;
    ++probe->calls;
    return 1;
}

static int
dc_hint(sixel_lookup_policy_interface_t const *self,
    unsigned char const *p, int hint)
{
    (void)hint;
    return dc_map(self, p);
}

/* One invocation is one backend/method/gate observation. The pixel at (0,0)
 * has frozen byte queries A=(96,106,115), X=(96,153,146), BN=(131,131,131).
 * BN uses mono taps 65+214: bias=24/255*32, truncated toward zero.
 * Float A/X biases are (-32,-22.75,-13.5) and (-32,25.25,18.5).
 * Each paint decision has a distance margin much larger than RGB101 rounding.
 * The CPU lookup stub returns the known candidate to isolate the keep gate;
 * encoder integration cases separately exercise actual palette selection.
 * gate: 0 bypass original-source early keep, 1 post-lookup replacement,
 * 2 query early keep, 3 query post-lookup tie, 4 positive-T query keep,
 * 5 refresh-selected T0, 6 invalid execution enum. */
int
delta_compare_pixel_test(int method, int native, int gpu, int gate)
{
    static unsigned char const queries[3][3] = {
        {96, 106, 115}, {96, 153, 146}, {131, 131, 131}
    };
    static char const *const classes[2][3] = {
        {"dither/a_dither.8bit", "dither/x_dither.8bit",
         "dither/bluenoise.8bit"},
        {"dither/a_dither.float32", "dither/x_dither.float32",
         "dither/bluenoise.float32"}
    };
    static int const methods[3] = {SIXEL_DIFFUSE_A_DITHER,
        SIXEL_DIFFUSE_X_DITHER, SIXEL_DIFFUSE_BLUENOISE_DITHER};
    sixel_factory_t *factory;
    sixel_dither_t *dither;
    sixel_dither_policy_interface_t *policy;
    sixel_dither_policy_prepare_request_t prepare;
    sixel_dither_policy_apply_request_t apply;
    sixel_dither_execution_t execution;
    sixel_palette_float32_entries_request_t float_palette;
    sixel_gpu_palette_request_t request;
    sixel_delta_mask_t selection;
    sixel_gpu_device_interface_t *device;
    sixel_gpu_lookup_preparer_interface_t *provider;
    sixel_gpu_lookup_plan_interface_t *plan;
    sixel_gpu_program_t *program;
    sixel_lookup_execution_request_t lookup;
    sixel_lookup_policy_vtbl_t vtbl;
    dc_lookup_t probe;
    unsigned char colors[9] = {255, 255, 255, 0, 0, 0, 0, 0, 0};
    unsigned char pixel[3] = {128, 128, 128};
    unsigned char retained[3];
    unsigned char valid = 1;
    unsigned char skip = 0;
    float source[3];
    float entries[9];
    sixel_index_t result = 99;
    void *object;
    SIXELSTATUS status;
    unsigned int threshold;
    int expected_index, expected_mask, expected_calls;
    int i, ok;

    dither = NULL;
    policy = NULL;
    factory = NULL;
    device = NULL;
    provider = NULL;
    plan = NULL;
    program = NULL;
    ok = 0;
    memset(&prepare, 0, sizeof(prepare));
    memset(&apply, 0, sizeof(apply));
    memset(&execution, 0, sizeof(execution));
    memset(&request, 0, sizeof(request));
    memset(&lookup, 0, sizeof(lookup));
    memset(&selection, 0, sizeof(selection));
    memset(&vtbl, 0, sizeof(vtbl));
    memset(&probe, 0, sizeof(probe));
    memcpy(colors + 3, queries[method], 3);
    memcpy(retained, gate == 0 ? pixel : queries[method], 3);
    if (gate == 1) memset(retained, method == 2 ? 126 : 130, 3);
    if (gate == 3) {
        retained[0] += 2;
        colors[3] -= 2;
    }
    if (gate == 4 || gate == 5) retained[0] += 2;
    threshold = gate == 4 || gate == 5 ? 3U : 0U;
    expected_index = gate == 2 || gate == 3 || gate == 4 ? 2 : 1;
    expected_mask = gate == 4 ? 3 : expected_index == 2 ? 1 : 0;
    expected_calls = gate == 2 || gate == 4 || gate == 6 ? 0 : 1;
    for (i = 0; i < 3; ++i) source[i] = 128.0f / 255.0f;
    DC_CHECK(sixel_dither_new(&dither, 3, NULL) == SIXEL_OK);
    sixel_dither_set_palette(dither, colors);
    if (native) {
        for (i = 0; i < 9; ++i) entries[i] = colors[i] / 255.0f;
        float_palette.entries = entries;
        float_palette.colors = 3;
        float_palette.depth = 3 * sizeof(float);
        DC_CHECK(dither->palette->vtbl->init_entries_float32(
            dither->palette, &float_palette) == SIXEL_OK);
        DC_CHECK(sixel_palette_prepare_rgb101(dither->palette,
            SIXEL_COLORSPACE_GAMMA, SIXEL_PALETTE_OUTPUT_LEGACY) == SIXEL_OK);
    }
    prepare.pixelformat = native ? SIXEL_PIXELFORMAT_RGBFLOAT32 :
        SIXEL_PIXELFORMAT_RGB888;
    prepare.method_for_scan = SIXEL_SCAN_RASTER;
    prepare.options.ncolors = 3;
    prepare.options.keycolor_reserved = 1;
    prepare.options.method_for_diffuse = methods[method];
    prepare.options.use_rgb101_palette = native;
    prepare.options.a_dither_strength = 1;
    prepare.options.x_dither_strength = 1;
    prepare.options.bluenoise_strength = 1;
    prepare.options.bluenoise_size = 64;
    execution.working_palette = dither->palette;
    execution.allocator = dither->allocator;
    execution.width = execution.height = 1;
    execution.keycolor = -1;
    execution.delta_enabled = 1;
    execution.delta_compare = gate == 6 ? (sixel_delta_compare_t)99 :
        SIXEL_DELTA_COMPARE_QUERY;
    execution.delta_threshold = threshold;
    execution.delta_keycolor = 2;
    execution.retained_rgb = retained;
    execution.retained_rgb_size = 3;
    execution.retained_valid = &valid;
    execution.retained_valid_size = 1;
    execution.plane_width = execution.plane_height = 1;
    execution.skip = &skip;
    execution.skip_size = 1;
    if (gate == 5) {
        selection.cells[0] = 1;
        selection.columns = selection.rows = 1;
        selection.width = selection.height = 1;
        execution.selection.words = selection.cells;
        execution.selection.word_count = 128;
        execution.selection.columns = execution.selection.rows = 1;
        execution.selection.width = execution.selection.height = 1;
    }
    if (gpu) {
        request.policy = SIXEL_GPU_POLICY_FORCE;
        request.dest = &result;
        request.pixels = pixel;
        request.pixel_count = 1;
        request.width = request.height = 1;
        request.pixelformat = SIXEL_PIXELFORMAT_RGB888;
        request.palette = colors;
        request.palette_size = 6;
        request.palette_depth = 3;
        request.ncolors = 2;
        request.lut_policy = SIXEL_LUT_POLICY_NONE;
        request.method_for_diffuse = methods[method];
        request.method_for_scan = SIXEL_SCAN_RASTER;
        request.positional_strength_override = 1;
        request.positional_strength = 1;
        request.bluenoise_strength_override = 1;
        request.bluenoise_strength = 1;
        request.bluenoise_phase_override = 1;
        request.bluenoise_channel_override = 1;
        request.bluenoise_gradient_factor_override = 1;
        request.sixdelta_enabled = request.has_6delta_accumulation = 1;
        request.accumulation_pixels = retained;
        request.accumulation_pixels_size = 3;
        request.accumulation_valid_mask = &valid;
        request.accumulation_valid_mask_size = 1;
        request.accumulation_keycolor = 2;
        request.sixdelta_threshold = threshold;
        request.sixdelta_compare = execution.delta_compare;
        request.accumulation_result_mask = &skip;
        request.accumulation_result_mask_size = 1;
        request.residual_mask = &selection;
        if (gpu == 2) {
            DC_CHECK(sixel_gpu_device_acquire(&device) == SIXEL_OK);
            DC_CHECK(sixel_gpu_lookup_ordered_create(dither->allocator,
                &provider) == SIXEL_OK);
            lookup.working_palette = dither->palette;
            lookup.ncolors = 2;
            lookup.pixelformat = SIXEL_PIXELFORMAT_RGB888;
            lookup.colorspace = SIXEL_COLORSPACE_GAMMA;
            DC_CHECK(provider->vtbl->prepare(provider, &lookup, device,
                &plan) == SIXEL_OK);
            DC_CHECK(sixel_gpu_program_prepare(device, plan, &program)
                == SIXEL_OK);
            request.program = program;
            DC_CHECK(sixel_gpu_palette_apply_prepared(&request) == SIXEL_OK);
        } else {
            DC_CHECK(sixel_gpu_palette_apply(&request) == SIXEL_OK);
        }
    } else {
        DC_CHECK(sixel_components_getservice("services/factory", &object)
            == SIXEL_OK);
        factory = object;
        DC_CHECK(factory->vtbl->create(factory, classes[native][method],
            dither->allocator, &object) == SIXEL_OK);
        policy = object;
        DC_CHECK(policy->vtbl->prepare(policy, &prepare) == SIXEL_OK);
        vtbl.map_pixel = dc_map;
        vtbl.map_pixel_hint = dc_hint;
        probe.iface.vtbl = &vtbl;
        apply.execution = &execution;
        apply.width = apply.height = 1;
        apply.depth = 3;
        apply.data = native ? (unsigned char *)(void *)source : pixel;
        apply.result = &result;
        apply.palette = colors;
        apply.lookup_policy = &probe.iface;
        status = policy->vtbl->apply(policy, &apply);
        if (gate == 6) {
            DC_CHECK(status == SIXEL_BAD_ARGUMENT && result == 99 &&
                skip == 0 && probe.calls == 0);
            ok = 1;
            goto end;
        }
        DC_CHECK(status == SIXEL_OK);
        DC_CHECK(probe.calls == expected_calls);
    }
    DC_CHECK(result == expected_index && skip == expected_mask);
    ok = 1;
end:
    sixel_gpu_program_destroy(program);
    if (plan != NULL) plan->vtbl->unref(plan);
    if (provider != NULL) provider->vtbl->unref(provider);
    if (device != NULL) device->vtbl->unref(device);
    if (policy != NULL) policy->vtbl->unref(policy);
    if (factory != NULL) factory->vtbl->unref(factory);
    sixel_dither_unref(dither);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* One option/state transition per caller; no pixel work or file I/O. */
int
delta_compare_option_test(char const *first, char const *second,
    char const *environment, int expected, int rejected, int state)
{
    sixel_encoder_t *encoder;
    SIXELSTATUS status;
    unsigned char rgb[3] = {102, 102, 102};
    unsigned char const *retained;
    int ok;

    encoder = NULL;
    retained = NULL;
    ok = 0;
    DC_CHECK(sixel_compat_setenv("SIXEL_UPDATE_DELTA_COMPARE", environment)
        == 0);
    DC_CHECK(sixel_encoder_new(&encoder, NULL) == SIXEL_OK);
    if (first != NULL) DC_CHECK(sixel_encoder_setopt(encoder,
        SIXEL_OPTFLAG_UPDATE_POLICY, first) == SIXEL_OK);
    if (state) {
        DC_CHECK(sixel_encoder_set_accumulation_buffer(encoder, rgb,
            1, 1, SIXEL_PIXELFORMAT_RGB888) == SIXEL_OK);
        retained = encoder->accumulation_pixels;
        encoder->residual_state.refresh_valid = 1;
    }
    if (second != NULL) {
        status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_UPDATE_POLICY,
            second);
        DC_CHECK(rejected ? SIXEL_FAILED(status) : status == SIXEL_OK);
    }
    DC_CHECK(encoder->sixdelta_compare == expected);
    if (state) {
        DC_CHECK(encoder->accumulation_valid &&
            encoder->accumulation_pixels == retained &&
            memcmp(retained, rgb, 3) == 0 &&
            encoder->residual_state.refresh_valid == (state == 1));
    }
    ok = 1;
end:
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

typedef struct dc_stream {
    unsigned char bytes[4096];
    size_t size;
} dc_stream_t;

static int
dc_write(char *bytes, int size, void *context)
{
    dc_stream_t *stream;

    stream = context;
    if (size < 0 || (size_t)size > sizeof(stream->bytes) - stream->size)
        return -1;
    memcpy(stream->bytes + stream->size, bytes, (size_t)size);
    stream->size += (size_t)size;
    return size;
}

/* Three source colors and five slots (including white and omission) give an
 * exact working palette 85,140,170. At the center, phase (47,52) gives query 109:
 * target 140 keeps retained 140, while query replaces it with emitted 84.
 * Reserved white prevents the exact-palette shortcut from disabling dither.
 * Observe the composed center through the real encoder/playback boundary. */
int
delta_compare_encode_test(int gpu, int queued, int query, int method,
    int partial, int limited)
{
    sixel_encoder_t *encoder;
    sixel_frame_t *frame;
    sixel_output_t *output;
    sixel_playback_t *playback;
    sixel_playback_config_t config;
    sixel_playback_options_t options;
    dc_stream_t stream;
    unsigned char pixel[9] = {85,85,85, 140,140,140, 170,170,170};
    unsigned char history[45];
    unsigned char reserved[3] = {255, 255, 255};
    unsigned char *decoded, *paint, *palette;
    unsigned int flags;
    int width, height, colors, ok, expected, offset;

    encoder = NULL;
    frame = NULL;
    output = NULL;
    playback = NULL;
    decoded = paint = palette = NULL;
    ok = 0;
    memset(&stream, 0, sizeof(stream));
    memset(history, 140, sizeof(history));
    expected = query && method == 0 ? 84 : 140;
    offset = partial ? 21 : 3;
    if (partial) {
        memset(history, 84, sizeof(history));
        memset(history + offset, 140, 3);
    }
    DC_CHECK(sixel_encoder_new(&encoder, NULL) == SIXEL_OK);
    DC_CHECK(sixel_encoder_setopt(encoder, 'p', "5") == SIXEL_OK);
    DC_CHECK(sixel_encoder_set_reserved_palette(encoder, reserved, 1)
        == SIXEL_OK);
    DC_CHECK(sixel_encoder_setopt(encoder, '4', "full-frame") == SIXEL_OK);
    DC_CHECK(sixel_encoder_setopt(encoder, '5', "none") == SIXEL_OK);
    DC_CHECK(sixel_encoder_setopt(encoder, 'F', "none") == SIXEL_OK);
    DC_CHECK(sixel_encoder_setopt(encoder, 'Q', "heckbert") == SIXEL_OK);
    DC_CHECK(sixel_encoder_setopt(encoder, 'd', method == 1 ? "none" :
        method == 2 ? "fs" :
        "bluenoise:T1:P47,52:Cmono:G0") == SIXEL_OK);
    DC_CHECK(sixel_encoder_setopt(encoder, '~', "ordered") == SIXEL_OK);
    DC_CHECK(sixel_encoder_setopt(encoder, '.', "8bit") == SIXEL_OK);
    DC_CHECK(sixel_encoder_setopt(encoder, '_', "none") == SIXEL_OK);
    DC_CHECK(sixel_encoder_setopt(encoder, 'X', "gamma") == SIXEL_OK);
    DC_CHECK(sixel_encoder_setopt(encoder, 'W', "gamma") == SIXEL_OK);
    DC_CHECK(sixel_encoder_setopt(encoder, 'U', "gamma") == SIXEL_OK);
    DC_CHECK(sixel_encoder_setopt(encoder, 't', "rgb") == SIXEL_OK);
    DC_CHECK(sixel_encoder_setopt(encoder, 'G', gpu ? "force" : "off")
        == SIXEL_OK);
    DC_CHECK(sixel_encoder_setopt(encoder, 'Z', limited ?
        "delta:Cq:T255" : query ? "delta:Cq" : "delta:Ct") == SIXEL_OK);
    DC_CHECK(sixel_encoder_set_accumulation_buffer(encoder, history,
        partial ? 5 : 3, partial ? 3 : 1, SIXEL_PIXELFORMAT_RGB888)
        == SIXEL_OK);
    if (partial) DC_CHECK(sixel_encoder_set_6delta_plane_origin(encoder,
        1, 1) == SIXEL_OK);
    if (limited) DC_CHECK(sixel_encoder_set_6delta_threshold_limit(encoder,
        0) == SIXEL_OK);
    DC_CHECK(sixel_frame_new(&frame, NULL) == SIXEL_OK);
    DC_CHECK(sixel_frame_init_borrowed(frame, pixel, 3, 1,
        SIXEL_PIXELFORMAT_RGB888, NULL, 0) == SIXEL_OK);
    DC_CHECK(sixel_output_new(&output, dc_write, &stream, NULL) == SIXEL_OK);
    if (queued) {
        DC_CHECK(sixel_playback_config_init(&config, sizeof(config))
            == SIXEL_OK);
        config.capacity = 2;
        config.palette_mode = queued == 2 ? SIXEL_PLAYBACK_PALETTE_LATEST :
            SIXEL_PLAYBACK_PALETTE_PER_FRAME;
        DC_CHECK(sixel_playback_new(&playback, encoder, output, &config, NULL)
            == SIXEL_OK);
        DC_CHECK(sixel_playback_options_init(&options, sizeof(options))
            == SIXEL_OK);
        DC_CHECK(sixel_playback_submit(playback, frame, SIXEL_PLAYBACK_WAIT,
            -1, &options) == SIXEL_OK);
        DC_CHECK(sixel_playback_drain(playback, -1) == SIXEL_OK);
    } else {
        DC_CHECK(sixel_encoder_encode_frame(encoder, frame, output)
            == SIXEL_OK);
    }
    if (!queued && encoder->accumulation_pixels[offset] != expected) {
        fprintf(stderr, "encoded retained=%d expected=%d compare=%d\n",
            encoder->accumulation_pixels[offset], expected,
            encoder->sixdelta_compare);
    }
    DC_CHECK(queued || (encoder->accumulation_valid &&
        encoder->accumulation_pixels[offset] == expected &&
        encoder->accumulation_pixels[offset + 1] == expected &&
        encoder->accumulation_pixels[offset + 2] == expected));
    if (expected == 84) {
        DC_CHECK(sixel_decode_raw_with_options_mask(stream.bytes,
            (int)stream.size, 0, &decoded, &paint, &width, &height,
            &palette, &colors, &flags, encoder->allocator) == SIXEL_OK);
        DC_CHECK(width == 3 && height == 1 && paint[1] &&
            decoded[1] < colors && palette[decoded[1] * 3] == 84 &&
            palette[decoded[1] * 3 + 1] == 84 &&
            palette[decoded[1] * 3 + 2] == 84);
    }
    ok = 1;
end:
    if (encoder != NULL) {
        sixel_allocator_free(encoder->allocator, decoded);
        sixel_allocator_free(encoder->allocator, paint);
        sixel_allocator_free(encoder->allocator, palette);
    }
    sixel_playback_unref(playback);
    sixel_output_unref(output);
    sixel_frame_unref(frame);
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

#undef DC_CHECK
