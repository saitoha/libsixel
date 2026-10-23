/* SPDX-License-Identifier: MIT */
/* Expose fileno when the test helper is compiled in strict C99 mode. */
#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder.h"
#include "src/progressive.h"
#include "src/sixel_decode_pixels.h"

static size_t context_test_live;
static size_t context_test_fail;
static size_t context_test_allocations;

static void *
context_test_malloc(size_t size)
{
    void *data;

    if (++context_test_allocations == context_test_fail) {
        return NULL;
    }
    data = malloc(size);
    context_test_live += data != NULL;
    return data;
}

static void *
context_test_calloc(size_t count, size_t size)
{
    void *data;

    if (++context_test_allocations == context_test_fail) {
        return NULL;
    }
    data = calloc(count, size);
    context_test_live += data != NULL;
    return data;
}

static void *
context_test_realloc(void *previous, size_t size)
{
    void *data;

    if (++context_test_allocations == context_test_fail) {
        return NULL;
    }
    data = realloc(previous, size);
    context_test_live += data != NULL && previous == NULL;
    return data;
}

static void
context_test_free(void *data)
{
    context_test_live -= data != NULL;
    free(data);
}

static int
context_test_write(char *data, int size, void *priv)
{
    (void)data;
    (void)size;
    (void)priv;
    return 0;
}

static int
context_test_cancel(void *priv)
{
    return *(int *)priv;
}

static sixel_frame_t *
context_test_frame(void const *data, int width, int height, int format)
{
    sixel_frame_t *frame;
    sixel_frame_interface_t *frame_if;
    sixel_frame_transparency_t transparency;
    unsigned char *owned;
    size_t size;

    frame = NULL;
    size = (size_t)width * height * sixel_helper_compute_depth(format);
    owned = malloc(size);
    if (owned == NULL) {
        return NULL;
    }
    memcpy(owned, data, size);
    if (sixel_frame_new(&frame, NULL) != SIXEL_OK ||
            sixel_frame_init(frame, owned, width, height, format, NULL, 0)
                != SIXEL_OK) {
        free(owned);
        sixel_frame_unref(frame);
        return NULL;
    }
    if (format == SIXEL_PIXELFORMAT_RGBA8888) {
        /* Raw frame storage needs explicit alpha interpretation metadata,
         * just as loader-produced frames do; pixel format alone is not it. */
        frame_if = sixel_frame_as_interface(frame);
        memset(&transparency, 0, sizeof(transparency));
        transparency.transparent = -1;
        transparency.alpha_zero_is_transparent = 1;
        if (frame_if->vtbl->set_transparency(frame_if, &transparency)
                != SIXEL_OK) {
            sixel_frame_unref(frame);
            return NULL;
        }
    }
    return frame;
}

static SIXELSTATUS
context_test_audit(sixel_repair_context_t *context, size_t quantum,
                    sixel_repair_audit_t *audit, size_t *visits)
{
    sixel_repair_work_t work;
    SIXELSTATUS status;
    size_t calls;

    *visits = 0;
    for (calls = 0; calls < 10000; ++calls) {
        memset(&work, 0, sizeof(work));
        work.pixel_limit = quantum;
        work.distance_limit = quantum;
        status = sixel_repair_context_audit(context, &work, audit);
        if (work.pixel_visits > quantum ||
                work.distance_evaluations > quantum) {
            return SIXEL_LOGIC_ERROR;
        }
        *visits += work.pixel_visits;
        if (status != SIXEL_OK || audit->outcome != SIXEL_REPAIR_YIELDED) {
            return status;
        }
        /* An unfinished scan cannot advertise a full-domain quality result. */
        if (audit->complete) {
            return SIXEL_LOGIC_ERROR;
        }
    }
    return SIXEL_RUNTIME_ERROR;
}

#define CONTEXT_REQUIRE(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "context case %d, line %d\n", mode, __LINE__); \
        goto end; \
    } \
} while (0)

int
test_progressive_context_case(int mode)
{
    static char const *spaces[] = {
        "gamma", "linear", "oklab", "cielab", "din99d"
    };
    sixel_encoder_t *encoder;
    sixel_allocator_t *allocator;
    sixel_repair_context_t *context;
    sixel_repair_context_t *second;
    sixel_frame_t *source;
    sixel_frame_t *update;
    sixel_frame_t *snapshot;
    sixel_output_t *output;
    sixel_repair_config_t config;
    sixel_repair_config_t invalid;
    sixel_repair_audit_t audit;
    sixel_repair_audit_t previous;
    sixel_repair_work_t work;
    struct {
        sixel_repair_config_t config;
        unsigned char tail[16];
    } guarded;
    unsigned char white[3] = { 255, 255, 255 };
    unsigned char black[3] = { 0, 0, 0 };
    unsigned char alpha[8] = { 255, 255, 255, 255, 0, 0, 0, 0 };
    unsigned char pixels[35 * 25 * 3];
    unsigned char history[35 * 25 * 3];
    unsigned char *retained;
    uint64_t history_revision;
    uint64_t configuration_revision;
    size_t i;
    size_t live;
    size_t visits;
    double expected;
    int width;
    int height;
    int cancel;
    int ok;
    SIXELSTATUS status;

    encoder = NULL;
    allocator = NULL;
    context = second = NULL;
    source = update = snapshot = NULL;
    output = NULL;
    ok = 0;
    cancel = 0;
    context_test_live = context_test_fail = context_test_allocations = 0;
    CONTEXT_REQUIRE(sixel_repair_config_init(&config, sizeof(config))
        == SIXEL_OK);
    config.stop_distance_auto = 0;
    if (mode == 17) {
        memset(&guarded, 0xa5, sizeof(guarded));
        CONTEXT_REQUIRE(sixel_repair_config_init(&guarded.config,
            sizeof(config) - 1) == SIXEL_BAD_ARGUMENT);
        for (i = 0; i < sizeof(guarded); ++i) {
            CONTEXT_REQUIRE(((unsigned char *)&guarded)[i] == 0xa5);
        }
        CONTEXT_REQUIRE(sixel_repair_config_init(&guarded.config,
            sizeof(guarded)) == SIXEL_OK);
        for (i = 0; i < sizeof(guarded.tail); ++i) {
            CONTEXT_REQUIRE(guarded.tail[i] == 0xa5);
        }
        CONTEXT_REQUIRE(guarded.config.stop_distance_auto == 1 &&
            guarded.config.max_colors == 32 &&
            guarded.config.sample_limit == 4096);
        ok = 1;
        goto end;
    }
    CONTEXT_REQUIRE(sixel_allocator_new(&allocator, context_test_malloc,
        context_test_calloc, context_test_realloc, context_test_free)
            == SIXEL_OK);
    CONTEXT_REQUIRE(sixel_encoder_new(&encoder, allocator) == SIXEL_OK);
    CONTEXT_REQUIRE(sixel_encoder_setopt(encoder,
        SIXEL_OPTFLAG_THREADING_POLICY, "1") == SIXEL_OK);
    CONTEXT_REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_GPU_POLICY,
        "off") == SIXEL_OK);
    CONTEXT_REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_UPDATE_POLICY,
        "delta") == SIXEL_OK);
    CONTEXT_REQUIRE(sixel_encoder_setopt(encoder,
        SIXEL_OPTFLAG_WORKING_COLORSPACE, mode >= 35 ? spaces[mode - 35]
                                                     : "gamma") == SIXEL_OK);
    if (mode == 33) {
        CONTEXT_REQUIRE(sixel_encoder_set_cancel_callback(encoder,
            context_test_cancel, &cancel) == SIXEL_OK);
    }
    width = mode == 29 || mode == 33 ? 35 : mode == 26 || mode == 27 ? 2 : 1;
    height = mode == 29 || mode == 33 ? 25 : 1;
    memset(pixels, 255, sizeof(pixels));
    memset(history, 0, sizeof(history));
    if (mode == 25) {
        pixels[1] = pixels[2] = 0;
    }
    if (mode == 27) {
        alpha[3] = 0;
    }
    source = context_test_frame(mode == 26 || mode == 27 ? alpha : pixels,
        width, height, mode == 26 || mode == 27
            ? SIXEL_PIXELFORMAT_RGBA8888 : SIXEL_PIXELFORMAT_RGB888);
    CONTEXT_REQUIRE(source != NULL);
    if (mode == 28 || mode == 30) {
        /* Bind one pixel at (1,1) in a 3x2 plane. Unknown pixels elsewhere
         * must not affect its audit or make binding resize the surface. */
        memcpy(history + 12, white, 3);
        CONTEXT_REQUIRE(sixel_encoder_set_accumulation_buffer(encoder,
            history, 3, 2, SIXEL_PIXELFORMAT_RGB888) == SIXEL_OK);
        encoder->accumulation_valid_mask[0] = 0;
    } else if (mode != 27) {
        CONTEXT_REQUIRE(sixel_encoder_set_accumulation_buffer(encoder,
            history, width, height, SIXEL_PIXELFORMAT_RGB888) == SIXEL_OK);
    }
    retained = encoder->accumulation_pixels;
    history_revision = encoder->history_revision;
    configuration_revision = encoder->configuration_revision;
    if (mode == 18) {
        for (i = 0; i < 12; ++i) {
            invalid = config;
            switch (i) {
            case 0: invalid.struct_size = 0; break;
            case 1: invalid.stop_distance = NAN; break;
            case 2: invalid.stop_distance = -1; break;
            case 3: invalid.selection_fraction = 0; break;
            case 4: invalid.max_colors = 256; break;
            case 5: invalid.sample_limit = 0; break;
            case 6: invalid.candidate_limit = 1; break;
            case 7: invalid.min_gain_ratio = 1; break;
            case 8: invalid.swap_limit = 17; break;
            case 9: invalid.noise_strength = INFINITY; break;
            case 10: invalid.noise_phase_x = 64; break;
            default: invalid.gpu_policy = 3; break;
            }
            CONTEXT_REQUIRE(sixel_repair_context_new(&context, encoder,
                &invalid, allocator) == SIXEL_BAD_ARGUMENT);
            CONTEXT_REQUIRE(context == NULL &&
                encoder->progressive_context == NULL &&
                encoder->accumulation_pixels == retained &&
                encoder->history_revision == history_revision);
        }
        ok = 1;
        goto end;
    }
    if (mode == 23) {
        live = context_test_live;
        context_test_fail = context_test_allocations + 1;
        status = sixel_repair_context_new(&context, encoder, &config,
                                           allocator);
        context_test_fail = 0;
        CONTEXT_REQUIRE(status == SIXEL_BAD_ALLOCATION && context == NULL &&
            encoder->progressive_context == NULL && context_test_live == live);
        ok = 1;
        goto end;
    }
    if (mode == 25) {
        config.stop_distance = 1;
    }
    CONTEXT_REQUIRE(sixel_repair_context_new(&context, encoder, &config,
        allocator) == SIXEL_OK);
    if (mode == 19) {
        CONTEXT_REQUIRE(sixel_repair_context_new(&second, encoder, &config,
            allocator) == SIXEL_BAD_ARGUMENT && second == NULL &&
            encoder->progressive_context == context);
        CONTEXT_REQUIRE(sixel_repair_context_destroy(context) == SIXEL_OK);
        context = NULL;
        CONTEXT_REQUIRE(encoder->progressive_context == NULL &&
            encoder->accumulation_pixels == retained &&
            encoder->history_revision == history_revision &&
            encoder->configuration_revision == configuration_revision);
        CONTEXT_REQUIRE(sixel_repair_context_new(&context, encoder, &config,
            allocator) == SIXEL_OK);
        ok = 1;
        goto end;
    }
    CONTEXT_REQUIRE(sixel_repair_context_bind(context, source,
        mode == 28 || mode == 30, mode == 28 || mode == 30, 1) == SIXEL_OK);
    snapshot = context->source;
    if (mode == 20) {
        memset(sixel_frame_get_pixels(source), 0, 3);
        CONTEXT_REQUIRE(((float *)sixel_frame_get_pixels(snapshot))[0] == 1);
        /* The context is the remaining owner of its encoder. */
        sixel_encoder_unref(encoder);
        encoder = NULL;
        CONTEXT_REQUIRE(context_test_audit(context, 100, &audit, &visits)
            == SIXEL_OK && audit.outcome == SIXEL_REPAIR_READY);
        ok = 1;
        goto end;
    }
    if (mode == 21 || mode == 22) {
        CONTEXT_REQUIRE(sixel_repair_context_bind(context, source,
            mode == 22 ? 1 : 0, 0, mode == 22 ? 2 : 1)
                == SIXEL_BAD_ARGUMENT);
        CONTEXT_REQUIRE(context->source == snapshot &&
            context->generation == 1 &&
            encoder->accumulation_pixels == retained);
        ok = 1;
        goto end;
    }
    if (mode == 24) {
        live = context_test_live;
        for (i = 1; i < 100; ++i) {
            context_test_fail = context_test_allocations + i;
            status = sixel_repair_context_bind(context, source, 0, 0, 2);
            context_test_fail = 0;
            if (status == SIXEL_OK) {
                break;
            }
            CONTEXT_REQUIRE(status == SIXEL_BAD_ALLOCATION &&
                context_test_live == live && context->source == snapshot &&
                context->generation == 1 &&
                encoder->accumulation_pixels == retained &&
                encoder->history_revision == history_revision);
        }
        CONTEXT_REQUIRE(i > 3 && i < 100 && context->generation == 2);
        ok = 1;
        goto end;
    }
    if (mode == 26) {
        encoder->accumulation_valid_mask[0] = 0;
    }
    if (mode == 33) {
        memset(&work, 0, sizeof(work));
        work.pixel_limit = work.distance_limit = 17;
        CONTEXT_REQUIRE(sixel_repair_context_audit(context, &work, &audit)
            == SIXEL_OK && audit.outcome == SIXEL_REPAIR_YIELDED);
        cancel = 1;
        CONTEXT_REQUIRE(context_test_audit(context, 17, &audit, &visits)
            == SIXEL_INTERRUPTED && audit.outcome == SIXEL_REPAIR_NONE &&
            visits == 0 && encoder->history_revision == history_revision);
        cancel = 0;
    }
    CONTEXT_REQUIRE(context_test_audit(context, mode == 29 ? 1 : 10000,
        &audit, &visits) == SIXEL_OK);
    CONTEXT_REQUIRE(audit.complete && audit.state_snapshot_valid &&
        encoder->accumulation_pixels == retained &&
        encoder->history_revision == history_revision);
    if (mode == 25) {
        CONTEXT_REQUIRE(audit.outcome == SIXEL_REPAIR_CONVERGED);
        CONTEXT_REQUIRE(sixel_repair_context_destroy(context) == SIXEL_OK);
        context = NULL;
        config.stop_distance = nextafter(1, 0);
        CONTEXT_REQUIRE(sixel_repair_context_new(&context, encoder, &config,
            allocator) == SIXEL_OK);
        CONTEXT_REQUIRE(sixel_repair_context_bind(context, source, 0, 0, 1)
            == SIXEL_OK);
        CONTEXT_REQUIRE(context_test_audit(context, 100, &audit, &visits)
            == SIXEL_OK && audit.outcome == SIXEL_REPAIR_READY);
    } else if (mode == 26) {
        CONTEXT_REQUIRE(audit.outcome == SIXEL_REPAIR_NEEDS_BASELINE &&
            audit.domain_count == 1 && audit.invalid_count == 1 &&
            audit.eligible_count == 0);
    } else if (mode == 27) {
        CONTEXT_REQUIRE(audit.outcome == SIXEL_REPAIR_CONVERGED &&
            audit.domain_count == 0 && audit.invalid_count == 0 &&
            audit.maximum_error == 0 && audit.sum_error == 0);
    } else if (mode == 28) {
        CONTEXT_REQUIRE(audit.outcome == SIXEL_REPAIR_CONVERGED &&
            audit.eligible_count == 1 && audit.invalid_count == 0 &&
            audit.sum_error == 0 && encoder->sixdelta_origin_x == 0);
    } else if (mode == 29 || mode == 33) {
        CONTEXT_REQUIRE(audit.outcome == SIXEL_REPAIR_READY &&
            audit.unresolved_count == 875 && audit.sum_error == 2625 &&
            visits == (mode == 29 ? 875U : 858U));
        previous = audit;
        CONTEXT_REQUIRE(context_test_audit(context, 10000, &audit, &visits)
            == SIXEL_OK && visits == 0 &&
            memcmp(&previous, &audit, sizeof(audit)) == 0);
    } else if (mode == 30) {
        update = context_test_frame(black, 1, 1, SIXEL_PIXELFORMAT_RGB888);
        CONTEXT_REQUIRE(update != NULL && sixel_output_new(&output,
            context_test_write, NULL, NULL) == SIXEL_OK);
        CONTEXT_REQUIRE(sixel_encoder_encode_frame(encoder, update, output)
            == SIXEL_OK);
        CONTEXT_REQUIRE(context_test_audit(context, 100, &audit, &visits)
            == SIXEL_OK && audit.outcome == SIXEL_REPAIR_NEEDS_SOURCE &&
            !audit.complete && visits == 0);
        /* A failed refresh cannot make the old target current again. */
        CONTEXT_REQUIRE(sixel_repair_context_bind(context, source, 99, 0, 2)
            == SIXEL_BAD_ARGUMENT);
        CONTEXT_REQUIRE(context_test_audit(context, 100, &audit, &visits)
            == SIXEL_OK && audit.outcome == SIXEL_REPAIR_NEEDS_SOURCE);
        CONTEXT_REQUIRE(sixel_repair_context_bind(context, source, 1, 1, 2)
            == SIXEL_OK);
        CONTEXT_REQUIRE(context_test_audit(context, 100, &audit, &visits)
            == SIXEL_OK && audit.outcome == SIXEL_REPAIR_CONVERGED);
    } else if (mode == 31) {
        CONTEXT_REQUIRE(sixel_encoder_setopt(encoder,
            SIXEL_OPTFLAG_WORKING_COLORSPACE, "linear") == SIXEL_OK);
        CONTEXT_REQUIRE(encoder->accumulation_pixels == retained);
        CONTEXT_REQUIRE(context_test_audit(context, 100, &audit, &visits)
            == SIXEL_OK && audit.outcome == SIXEL_REPAIR_NEEDS_SOURCE &&
            visits == 0 && !audit.complete);
    } else if (mode == 32 || mode == 34) {
        if (mode == 34) {
            encoder->history_revision = UINT64_MAX;
        }
        CONTEXT_REQUIRE(sixel_encoder_invalidate_6delta_plane(encoder)
            == SIXEL_OK);
        CONTEXT_REQUIRE(context_test_audit(context, 100, &audit, &visits)
            == SIXEL_OK && audit.outcome == (mode == 34
                ? SIXEL_REPAIR_NEEDS_SOURCE : SIXEL_REPAIR_NEEDS_BASELINE));
        CONTEXT_REQUIRE(context->source == snapshot);
        if (mode == 32) {
            CONTEXT_REQUIRE(sixel_encoder_set_accumulation_buffer(encoder,
                white, 1, 1, SIXEL_PIXELFORMAT_RGB888) == SIXEL_OK);
            CONTEXT_REQUIRE(context_test_audit(context, 100, &audit, &visits)
                == SIXEL_OK && audit.outcome == SIXEL_REPAIR_CONVERGED &&
                audit.sum_error == 0 && visits == 1);
        }
    } else if (mode >= 35) {
        /* White-to-black has squared length 3 in RGB and approximately 1
         * in normalized Lab-family coordinates. No implementation helper
         * computes the expected metric or normalizes opponent axes here. */
        expected = mode < 37 ? 3 : 1;
        CONTEXT_REQUIRE(audit.outcome == SIXEL_REPAIR_READY &&
            fabs(audit.sum_error - expected) < 0.0001 &&
            audit.maximum_error == audit.sum_error);
    }
    ok = 1;
end:
    context_test_fail = 0;
    (void)sixel_repair_context_destroy(second);
    (void)sixel_repair_context_destroy(context);
    sixel_output_unref(output);
    sixel_frame_unref(update);
    sixel_frame_unref(source);
    sixel_encoder_unref(encoder);
    sixel_allocator_unref(allocator);
    return ok && context_test_live == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

static SIXELSTATUS
context_test_prepare(sixel_repair_context_t *context, size_t quantum,
    sixel_repair_audit_t *audit, size_t totals[2])
{
    sixel_repair_work_t work;
    SIXELSTATUS status;
    size_t calls;

    totals[0] = totals[1] = 0;
    for (calls = 0; calls < 100000; ++calls) {
        memset(&work, 0, sizeof(work));
        work.pixel_limit = work.distance_limit = quantum;
        status = sixel_repair_context_prepare(context, &work, audit);
        if (work.pixel_visits > quantum ||
                work.distance_evaluations > quantum) {
            return SIXEL_LOGIC_ERROR;
        }
        totals[0] += work.pixel_visits;
        totals[1] += work.distance_evaluations;
        if (status != SIXEL_OK || audit->outcome != SIXEL_REPAIR_YIELDED) {
            return status;
        }
    }
    return SIXEL_RUNTIME_ERROR;
}

int
test_progressive_prepare_case(int mode)
{
    sixel_encoder_t *encoder;
    sixel_allocator_t *allocator;
    sixel_repair_context_t *context;
    sixel_frame_t *source;
    sixel_output_t *output;
    sixel_repair_config_t config;
    sixel_repair_audit_t audit;
    sixel_repair_work_t work;
    sixel_palette_emitted_color_t colors[2];
    sixel_repair_gpu_lookup_t lookup;
    sixel_repair_gpu_match_t match;
    float pixels[6] = { 1, 0, 0, 0, 1, 0 };
    unsigned char history[6] = { 0, 0, 0, 0, 0, 0 };
    unsigned char paint[2];
    unsigned char indices[2];
    unsigned char *retained;
    uint64_t revision;
    size_t totals[2];
    size_t replay_totals[2];
    size_t i;
    int cancel;
    int ok;
    SIXELSTATUS status;

    encoder = NULL;
    allocator = NULL;
    context = NULL;
    source = NULL;
    output = NULL;
    ok = cancel = 0;
    context_test_live = context_test_fail = context_test_allocations = 0;
    CONTEXT_REQUIRE(sixel_allocator_new(&allocator, context_test_malloc,
        context_test_calloc, context_test_realloc, context_test_free)
        == SIXEL_OK);
    CONTEXT_REQUIRE(sixel_encoder_new(&encoder, allocator) == SIXEL_OK);
    CONTEXT_REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_UPDATE_POLICY,
        "delta") == SIXEL_OK);
    CONTEXT_REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_GPU_POLICY,
        "off") == SIXEL_OK);
    CONTEXT_REQUIRE(sixel_encoder_setopt(encoder,
        SIXEL_OPTFLAG_WORKING_COLORSPACE, "gamma") == SIXEL_OK);
    CONTEXT_REQUIRE(sixel_repair_config_init(&config, sizeof(config))
        == SIXEL_OK);
    config.stop_distance_auto = 0;
    config.noise_strength = 0;
    config.selection_fraction = 1;
    config.max_colors = 2;
    if (mode == 62) {
        for (i = 0; i < 6; ++i) {
            pixels[i] = .351f;
            history[i] = 89;
        }
    } else if (mode == 63) {
        pixels[0] = 2;
        pixels[1] = pixels[2] = 0;
        pixels[3] = pixels[4] = pixels[5] = .5f;
        history[0] = 255;
        config.selection_fraction = .5;
        config.max_colors = config.sample_limit = 1;
    } else if (mode == 67) {
        config.gpu_policy = SIXEL_GPU_POLICY_FORCE;
    }
    source = context_test_frame(pixels, 2, 1, SIXEL_PIXELFORMAT_RGBFLOAT32);
    CONTEXT_REQUIRE(source != NULL);
    CONTEXT_REQUIRE(sixel_encoder_set_accumulation_buffer(encoder, history,
        2, 1, SIXEL_PIXELFORMAT_RGB888) == SIXEL_OK);
    CONTEXT_REQUIRE(sixel_encoder_set_cancel_callback(encoder,
        context_test_cancel, &cancel) == SIXEL_OK);
    CONTEXT_REQUIRE(sixel_repair_context_new(&context, encoder, &config,
        allocator) == SIXEL_OK);
    CONTEXT_REQUIRE(sixel_repair_context_bind(context, source, 0, 0, 1)
        == SIXEL_OK);
    retained = encoder->accumulation_pixels;
    revision = encoder->history_revision;
    if (mode == 66) {
        memset(&work, 0, sizeof(work));
        work.pixel_limit = work.distance_limit = 5;
        CONTEXT_REQUIRE(sixel_repair_context_prepare(context, &work, &audit)
            == SIXEL_OK && audit.outcome == SIXEL_REPAIR_YIELDED);
        cancel = 1;
        CONTEXT_REQUIRE(context_test_prepare(context, 1, &audit, totals)
            == SIXEL_INTERRUPTED && audit.outcome == SIXEL_REPAIR_NONE &&
            totals[0] == 0 && totals[1] == 0);
        cancel = 0;
    }
    if (mode == 68) {
        /* Fail successive allocation boundaries, resume the same context,
         * and prove that speculative allocation never commits history. */
        for (i = 1; i < 40; ++i) {
            context_test_fail = context_test_allocations + i;
            status = context_test_prepare(context, 10000, &audit, totals);
            context_test_fail = 0;
            CONTEXT_REQUIRE(encoder->accumulation_pixels == retained &&
                encoder->history_revision == revision &&
                memcmp(retained, history, sizeof(history)) == 0);
            if (status == SIXEL_OK) {
                break;
            }
            CONTEXT_REQUIRE(status == SIXEL_BAD_ALLOCATION);
            CONTEXT_REQUIRE(context_test_prepare(context, 1, &audit, totals)
                == SIXEL_OK && audit.outcome == SIXEL_REPAIR_READY &&
                context->application.painted == 2);
            CONTEXT_REQUIRE(sixel_repair_context_bind(context, source, 0, 0,
                i + 1) == SIXEL_OK);
        }
        CONTEXT_REQUIRE(i > 1 && i < 40);
    } else {
        CONTEXT_REQUIRE(context_test_prepare(context, 1, &audit, totals)
            == SIXEL_OK);
    }
    CONTEXT_REQUIRE(encoder->accumulation_pixels == retained &&
        encoder->history_revision == revision &&
        memcmp(retained, history, sizeof(history)) == 0);
    if (mode == 62) {
        CONTEXT_REQUIRE(audit.outcome == SIXEL_REPAIR_STALLED &&
            audit.unresolved_count == 2 && audit.sum_error > 0 &&
            context->full_selection && context->zero_noise &&
            context->rescue_used && context->rescue_position == 2);
    } else if (mode == 63) {
        CONTEXT_REQUIRE(audit.outcome == SIXEL_REPAIR_READY &&
            context->full_selection && context->application.painted == 1 &&
            context->paint[0] == 0 && context->paint[1] == 1);
    } else if (mode == 67 && audit.outcome == SIXEL_REPAIR_UNSUPPORTED) {
        lookup.queries = lookup.palette = pixels;
        lookup.count = lookup.colors = 1;
        lookup.policy = SIXEL_GPU_POLICY_FORCE;
        CONTEXT_REQUIRE(context->selection == NULL && context->gpu != NULL);
        CONTEXT_REQUIRE(sixel_repair_gpu_lookup(context->gpu, &lookup, &match)
            == SIXEL_FEATURE_ERROR);
    } else {
        CONTEXT_REQUIRE(audit.outcome == SIXEL_REPAIR_READY &&
            context->application.painted == 2 && context->solver.colors == 2);
        CONTEXT_REQUIRE(context->paint[0] && context->paint[1]);
        CONTEXT_REQUIRE(mode != 67 || context->application.gpu_used);
        CONTEXT_REQUIRE(memcmp(context->application.colors[
            context->application.palette[context->indices[0]]].rgb,
            "\377\0\0", 3) == 0);
        CONTEXT_REQUIRE(memcmp(context->application.colors[
            context->application.palette[context->indices[1]]].rgb,
            "\0\377\0", 3) == 0);
    }
    if (mode == 61) {
        memcpy(paint, context->paint, sizeof(paint));
        memcpy(indices, context->indices, sizeof(indices));
        for (i = 0; i < 2; ++i) {
            colors[i] = context->application.colors[
                context->application.palette[i]];
        }
        CONTEXT_REQUIRE(sixel_repair_context_bind(context, source, 0, 0, 2)
            == SIXEL_OK);
        CONTEXT_REQUIRE(context_test_prepare(context, 10000, &audit,
            replay_totals) == SIXEL_OK && audit.outcome == SIXEL_REPAIR_READY);
        CONTEXT_REQUIRE(memcmp(totals, replay_totals, sizeof(totals)) == 0 &&
            memcmp(paint, context->paint, sizeof(paint)) == 0 &&
            memcmp(indices, context->indices, sizeof(indices)) == 0);
        for (i = 0; i < 2; ++i) {
            CONTEXT_REQUIRE(memcmp(colors + i, context->application.colors +
                context->application.palette[i], sizeof(colors[i])) == 0);
        }
    } else if (mode == 64) {
        CONTEXT_REQUIRE(sixel_output_new(&output, context_test_write, NULL,
            allocator) == SIXEL_OK);
        CONTEXT_REQUIRE(sixel_encoder_encode_frame(encoder, source, output)
            == SIXEL_OK);
        CONTEXT_REQUIRE(context_test_prepare(context, 10000, &audit, totals)
            == SIXEL_OK && audit.outcome == SIXEL_REPAIR_NEEDS_SOURCE &&
            context->selection == NULL && context->application.painted == 0);
    } else if (mode == 65) {
        CONTEXT_REQUIRE(sixel_encoder_invalidate_6delta_plane(encoder)
            == SIXEL_OK);
        CONTEXT_REQUIRE(context_test_prepare(context, 10000, &audit, totals)
            == SIXEL_OK && audit.outcome == SIXEL_REPAIR_NEEDS_BASELINE &&
            context->selection == NULL && context->application.painted == 0);
    }
    ok = 1;
end:
    context_test_fail = 0;
    (void)sixel_repair_context_destroy(context);
    sixel_frame_unref(source);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    sixel_allocator_unref(allocator);
    return ok && context_test_live == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

typedef struct context_step_sink {
    unsigned char bytes[8192];
    size_t size;
    size_t calls;
    size_t allocations;
    sixel_encoder_t *encoder;
    int *cancel;
    int mode;
    int reentry_rejected;
} context_step_sink_t;

static int
context_step_write(char *data, int size, void *priv)
{
    context_step_sink_t *sink;

    sink = priv;
    ++sink->calls;
    if (size < 0 || (size_t)size > sizeof(sink->bytes) - sink->size) {
        return -1;
    }
    memcpy(sink->bytes + sink->size, data, (size_t)size);
    sink->size += (size_t)size;
    if (sink->mode == 85) {
        *sink->cancel = 1;
    } else if (sink->mode == 86) {
        sink->reentry_rejected = sixel_encoder_setopt(sink->encoder,
            SIXEL_OPTFLAG_WORKING_COLORSPACE, "linear") == SIXEL_RUNTIME_ERROR;
    } else if (sink->mode == 89) {
        sink->allocations = context_test_allocations;
        context_test_fail = context_test_allocations + 1;
    }
    return sink->mode == 82 ? -1 : 0;
}

static SIXELSTATUS
context_test_step(sixel_repair_context_t *context, sixel_output_t *output,
    size_t quantum, size_t bytes, sixel_repair_result_t *result)
{
    sixel_repair_work_t work;
    SIXELSTATUS status;
    size_t calls;

    for (calls = 0; calls < 100000; ++calls) {
        memset(&work, 0, sizeof(work));
        work.pixel_limit = work.distance_limit = quantum;
        status = sixel_repair_context_step(context, &work, bytes, output,
                                            result);
        if (work.pixel_visits > quantum ||
                work.distance_evaluations > quantum) {
            return SIXEL_LOGIC_ERROR;
        }
        if (status != SIXEL_OK ||
                result->audit.outcome != SIXEL_REPAIR_YIELDED) {
            return status;
        }
        if (result->painted || result->packet_bytes) {
            return SIXEL_LOGIC_ERROR;
        }
    }
    return SIXEL_RUNTIME_ERROR;
}

int
test_progressive_step_case(int mode)
{
    static char const baseline[] =
        "\033P0;1q\"1;1;4;1#0;2;0;0;0#0!4@\033\\";
    static char const wider_baseline[] =
        "\033P0;1q\"1;1;8;1#0;2;0;0;0#0!8@\033\\";
    sixel_encoder_t *encoder;
    sixel_allocator_t *allocator;
    sixel_repair_context_t *context;
    sixel_frame_t *source;
    sixel_output_t *output;
    sixel_output_t *ordinary;
    sixel_repair_config_t config;
    sixel_repair_result_t result;
    sixel_repair_work_t work;
    context_step_sink_t sink;
    unsigned char target[24] = { 255, 0, 0, 0, 255, 0,
                                0, 0, 255, 255, 255, 255 };
    unsigned char black[24] = { 0 };
    unsigned char expected[4] = { 0, 0, 0, 255 };
    unsigned char *decoded;
    uint64_t revision;
    size_t bytes;
    size_t size;
    size_t passes;
    size_t i;
    size_t totals[2];
    unsigned int flags;
    int width;
    int height;
    int source_width;
    int cancel;
    int ok;
    SIXELSTATUS status;

    encoder = NULL;
    allocator = NULL;
    context = NULL;
    source = NULL;
    output = ordinary = NULL;
    decoded = NULL;
    ok = cancel = 0;
    context_test_live = context_test_fail = context_test_allocations = 0;
    memset(&sink, 0, sizeof(sink));
    sink.mode = mode;
    sink.cancel = &cancel;
    memcpy(sink.bytes, baseline, sizeof(baseline) - 1);
    sink.size = sizeof(baseline) - 1;
    source_width = mode == 90 ? 8 : 4;
    if (mode == 90) {
        memcpy(sink.bytes, wider_baseline, sizeof(wider_baseline) - 1);
        sink.size = sizeof(wider_baseline) - 1;
        for (i = 0; i < 24; ++i) {
            target[i] = (unsigned char)((i / 3 + 1) * 255 / 8);
        }
    }
    CONTEXT_REQUIRE(sixel_allocator_new(&allocator, context_test_malloc,
        context_test_calloc, context_test_realloc, context_test_free)
        == SIXEL_OK);
    CONTEXT_REQUIRE(sixel_encoder_new(&encoder, allocator) == SIXEL_OK);
    sink.encoder = encoder;
    CONTEXT_REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_UPDATE_POLICY,
        "delta") == SIXEL_OK);
    CONTEXT_REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_GPU_POLICY,
        "off") == SIXEL_OK);
    CONTEXT_REQUIRE(sixel_encoder_setopt(encoder,
        SIXEL_OPTFLAG_WORKING_COLORSPACE, "gamma") == SIXEL_OK);
    CONTEXT_REQUIRE(sixel_encoder_set_cancel_callback(encoder,
        context_test_cancel, &cancel) == SIXEL_OK);
    CONTEXT_REQUIRE(sixel_output_new(&output, context_step_write, &sink,
        allocator) == SIXEL_OK);
    CONTEXT_REQUIRE(sixel_repair_config_init(&config, sizeof(config))
        == SIXEL_OK);
    config.stop_distance_auto = 0;
    config.noise_strength = 0;
    config.max_colors = (unsigned int)source_width;
    config.selection_fraction = mode == 78 ? .25 : 1;
    source = context_test_frame(target, source_width, 1,
                                SIXEL_PIXELFORMAT_RGB888);
    CONTEXT_REQUIRE(source != NULL);
    CONTEXT_REQUIRE(sixel_encoder_set_accumulation_buffer(encoder, black,
        source_width, 1, SIXEL_PIXELFORMAT_RGB888) == SIXEL_OK);
    CONTEXT_REQUIRE(sixel_repair_context_new(&context, encoder, &config,
        allocator) == SIXEL_OK);
    CONTEXT_REQUIRE(sixel_repair_context_bind(context, source, 0, 0, 1)
        == SIXEL_OK);
    revision = encoder->history_revision;
    bytes = mode == 79 || mode == 90 ? 50 : mode == 80 ? 1 : 4096;
    if (mode == 84) {
        cancel = 1;
        CONTEXT_REQUIRE(context_test_step(context, output, 1, bytes, &result)
            == SIXEL_INTERRUPTED && !sink.calls &&
            encoder->history_revision == revision);
        cancel = 0;
    } else if (mode == 87 || mode == 88) {
        memset(&work, 0, sizeof(work));
        work.pixel_limit = work.distance_limit = 1;
        CONTEXT_REQUIRE(sixel_repair_context_step(context, &work, bytes,
            output, &result) == SIXEL_OK &&
            result.audit.outcome == SIXEL_REPAIR_YIELDED && !sink.calls);
        if (mode == 87) {
            CONTEXT_REQUIRE(sixel_output_new(&ordinary, context_test_write,
                NULL, allocator) == SIXEL_OK);
            CONTEXT_REQUIRE(sixel_encoder_encode_frame(encoder, source,
                ordinary) == SIXEL_OK);
        } else {
            CONTEXT_REQUIRE(sixel_encoder_invalidate_6delta_plane(encoder)
                == SIXEL_OK);
        }
        CONTEXT_REQUIRE(context_test_step(context, output, 1, bytes, &result)
            == SIXEL_OK && !sink.calls && result.audit.outcome ==
                (mode == 87 ? SIXEL_REPAIR_NEEDS_SOURCE
                            : SIXEL_REPAIR_NEEDS_BASELINE));
        ok = 1;
        goto end;
    }
    encoder->sixdelta_refinement_needed = 1;
    if (mode == 91) {
        /* Start at the completed application artifact and fault every
         * packet/output-object/commit-record allocation, then retry it. */
        CONTEXT_REQUIRE(context_test_prepare(context, 10000, &result.audit,
            totals) == SIXEL_OK && result.audit.outcome == SIXEL_REPAIR_READY);
        for (i = 1; i < 100; ++i) {
            context_test_fail = context_test_allocations + i;
            status = context_test_step(context, output, 10000, bytes, &result);
            context_test_fail = 0;
            if (status == SIXEL_OK) {
                break;
            }
            CONTEXT_REQUIRE(status == SIXEL_BAD_ALLOCATION && !sink.calls &&
                encoder->history_revision == revision &&
                memcmp(encoder->accumulation_pixels, black, 12) == 0);
            CONTEXT_REQUIRE(context_test_step(context, output, 1, bytes,
                &result) == SIXEL_OK &&
                result.audit.outcome == SIXEL_REPAIR_FRAME_EMITTED);
            sink.size = sizeof(baseline) - 1;
            sink.calls = 0;
            CONTEXT_REQUIRE(sixel_encoder_set_accumulation_buffer(encoder,
                black, 4, 1, SIXEL_PIXELFORMAT_RGB888) == SIXEL_OK);
            CONTEXT_REQUIRE(sixel_repair_context_bind(context, source, 0, 0,
                i + 1) == SIXEL_OK);
            CONTEXT_REQUIRE(context_test_prepare(context, 10000, &result.audit,
                totals) == SIXEL_OK &&
                result.audit.outcome == SIXEL_REPAIR_READY);
            revision = encoder->history_revision;
        }
        CONTEXT_REQUIRE(i > 3 && i < 100);
    } else {
        status = context_test_step(context, output, mode == 81 ? 1 : 10000,
                                   bytes, &result);
    }
    if (mode == 82) {
        CONTEXT_REQUIRE(SIXEL_FAILED(status) && sink.calls == 1 &&
            !encoder->accumulation_valid &&
            encoder->history_revision > revision && result.painted == 0 &&
            result.packet_bytes == 0);
        CONTEXT_REQUIRE(context_test_step(context, output, 1, bytes, &result)
            == SIXEL_OK && result.audit.outcome == SIXEL_REPAIR_NEEDS_BASELINE);
        ok = 1;
        goto end;
    }
    CONTEXT_REQUIRE(status == SIXEL_OK);
    if (mode == 80) {
        CONTEXT_REQUIRE(result.audit.outcome == SIXEL_REPAIR_BUDGET_TOO_SMALL &&
            result.required_bytes > 1 && !sink.calls &&
            encoder->history_revision == revision &&
            encoder->sixdelta_refinement_needed == 1 &&
            memcmp(encoder->accumulation_pixels, black, 12) == 0);
        bytes = result.required_bytes;
        CONTEXT_REQUIRE(context_test_step(context, output, 1, bytes, &result)
            == SIXEL_OK);
    }
    CONTEXT_REQUIRE(result.audit.outcome == SIXEL_REPAIR_FRAME_EMITTED &&
        sink.calls == 1 && result.painted > 0 && result.packet_bytes <= bytes &&
        !result.audit.complete && encoder->history_revision == revision + 1 &&
        sixel_encoder_get_6delta_painted_pixels(encoder) == result.painted &&
        !sixel_encoder_needs_6delta_refinement(encoder));
    if (mode == 90) {
        /* Eight accepted pixels take three merge rounds. Repeated halving
         * must keep the sorted buffer even when its final storage swapped. */
        CONTEXT_REQUIRE(result.painted == 1 &&
            memcmp(encoder->accumulation_pixels, black, 21) == 0 &&
            memcmp(encoder->accumulation_pixels + 21, target + 21, 3) == 0);
    } else if (mode == 79 || mode == 80) {
        CONTEXT_REQUIRE(result.painted == 1 &&
            memcmp(encoder->accumulation_pixels, black, 9) == 0 &&
            memcmp(encoder->accumulation_pixels + 9, target + 9, 3) == 0);
    } else if (mode == 85) {
        size = sink.size;
        CONTEXT_REQUIRE(context_test_step(context, output, 1, bytes, &result)
            == SIXEL_INTERRUPTED && sink.size == size &&
            memcmp(encoder->accumulation_pixels, target, 12) == 0);
        cancel = 0;
    } else if (mode == 86) {
        CONTEXT_REQUIRE(sink.reentry_rejected);
    } else if (mode == 89) {
        CONTEXT_REQUIRE(context_test_allocations == sink.allocations);
        context_test_fail = 0;
    }
    if (mode == 78 || mode == 81) {
        for (i = 0; i < 8; ++i) {
            CONTEXT_REQUIRE(context_test_step(context, output,
                mode == 81 ? 1 : 10000, bytes, &result) == SIXEL_OK);
            if (result.audit.outcome == SIXEL_REPAIR_CONVERGED) {
                break;
            }
            CONTEXT_REQUIRE(result.audit.outcome == SIXEL_REPAIR_FRAME_EMITTED);
        }
        CONTEXT_REQUIRE(i < 8 && result.audit.complete &&
            result.audit.unresolved_count == 0 && result.audit.sum_error == 0 &&
            memcmp(encoder->accumulation_pixels, target, 12) == 0);
    }
    CONTEXT_REQUIRE(sixel_decode_sequence_mapped(sink.bytes, sink.size, NULL,
        &decoded, &width, &height, &flags, &passes, allocator) == SIXEL_OK);
    CONTEXT_REQUIRE(width == source_width && height == 1 &&
        passes == sink.calls + 1);
    for (i = 0; i < (size_t)source_width; ++i) {
        memcpy(expected, encoder->accumulation_pixels + i * 3, 3);
        CONTEXT_REQUIRE(memcmp(decoded + i * 4, expected, 4) == 0);
    }
    if (mode == 78) {
        CONTEXT_REQUIRE(sixel_output_new(&ordinary, context_test_write, NULL,
            allocator) == SIXEL_OK);
        CONTEXT_REQUIRE(sixel_encoder_encode_frame(encoder, source, ordinary)
            == SIXEL_OK && sixel_encoder_get_6delta_painted_pixels(encoder)
                == 0);
        size = sink.size;
        CONTEXT_REQUIRE(context_test_step(context, output, 1, bytes, &result)
            == SIXEL_OK && result.audit.outcome == SIXEL_REPAIR_NEEDS_SOURCE &&
            sink.size == size);
    }
    ok = 1;
end:
    context_test_fail = 0;
    sixel_allocator_free(allocator, decoded);
    (void)sixel_repair_context_destroy(context);
    sixel_frame_unref(source);
    sixel_output_unref(output);
    sixel_output_unref(ordinary);
    sixel_encoder_unref(encoder);
    sixel_allocator_unref(allocator);
    return ok && context_test_live == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

#if SIXEL_ENABLE_THREADS
typedef struct context_order_probe {
    sixel_encoder_t *encoder;
    sixel_repair_context_t *context;
    sixel_frame_t *frame;
    sixel_output_t *ordinary;
    sixel_thread_t worker;
    int after_admission;
    int launched;
    int registered;
    int emitting;
    size_t calls;
    size_t ordinary_painted;
    SIXELSTATUS status;
} context_order_probe_t;

static int
context_order_update(void *priv)
{
    context_order_probe_t *probe;

    probe = priv;
    probe->status = sixel_encoder_encode_frame(probe->encoder, probe->frame,
                                                probe->ordinary);
    probe->ordinary_painted =
        sixel_encoder_get_6delta_painted_pixels(probe->encoder);
    return 0;
}

static void
context_order_register(context_order_probe_t *probe)
{
    if (probe->launched) {
        return;
    }
    probe->launched = sixel_thread_create(&probe->worker,
        context_order_update, probe) == 0;
    if (!probe->launched) {
        return;
    }
    /* Observe the normal operation's registration while it is blocked on
     * this admitted operation. No elapsed-time assumptions order the race. */
    sixel_mutex_lock(&probe->encoder->operation_mutex);
    while (!probe->encoder->operation_waiters) {
        sixel_cond_wait(&probe->encoder->operation_cond,
                         &probe->encoder->operation_mutex);
    }
    probe->emitting = probe->encoder->operation_emitting;
    probe->registered = 1;
    sixel_mutex_unlock(&probe->encoder->operation_mutex);
}

static int
context_order_cancel(void *priv)
{
    context_order_probe_t *probe;

    probe = priv;
    if (!probe->after_admission && probe->context->commit_count == 4) {
        context_order_register(probe);
    }
    return 0;
}

static int
context_order_write(char *data, int size, void *priv)
{
    context_order_probe_t *probe;

    (void)data;
    (void)size;
    probe = priv;
    ++probe->calls;
    if (probe->after_admission) {
        context_order_register(probe);
    }
    return 0;
}
#endif

int
test_progressive_boundary_case(int mode)
{
    sixel_encoder_t *encoder;
    sixel_repair_context_t *context;
    sixel_frame_t *source;
    sixel_output_t *output;
    sixel_output_t *ordinary;
    sixel_repair_config_t config;
    sixel_repair_result_t result;
    unsigned char rgba[16] = { 255, 0, 0, 255, 0, 255, 0, 255,
                              0, 0, 255, 0, 255, 255, 255, 255 };
    unsigned char history[6 * 3 * 3];
    unsigned char expected[3];
    size_t i;
    int ok;
#if SIXEL_ENABLE_THREADS
    context_order_probe_t probe;
    sixel_repair_work_t work;
#endif

#if !SIXEL_ENABLE_THREADS
    if (mode != 92) {
        return 77;
    }
#endif
    encoder = NULL;
    context = NULL;
    source = NULL;
    output = ordinary = NULL;
    ok = 0;
#if SIXEL_ENABLE_THREADS
    memset(&probe, 0, sizeof(probe));
#endif
    CONTEXT_REQUIRE(sixel_encoder_new(&encoder, NULL) == SIXEL_OK);
    CONTEXT_REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_UPDATE_POLICY,
        "delta") == SIXEL_OK);
    CONTEXT_REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_GPU_POLICY,
        "off") == SIXEL_OK);
    CONTEXT_REQUIRE(sixel_encoder_setopt(encoder,
        SIXEL_OPTFLAG_WORKING_COLORSPACE, "gamma") == SIXEL_OK);
    CONTEXT_REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_ALPHA_POLICY,
        "keep") == SIXEL_OK);
    CONTEXT_REQUIRE(sixel_encoder_setopt(encoder,
        SIXEL_OPTFLAG_THREADING_POLICY, "1") == SIXEL_OK);
    memset(history, 0, sizeof(history));
    if (mode == 92) {
        for (i = 0; i < sizeof(history); i += 3) {
            history[i + 2] = 255;
        }
    } else {
        rgba[11] = 255;
    }
    source = context_test_frame(rgba, 4, 1, SIXEL_PIXELFORMAT_RGBA8888);
    CONTEXT_REQUIRE(source != NULL);
    CONTEXT_REQUIRE(sixel_encoder_set_accumulation_buffer(encoder, history,
        mode == 92 ? 6 : 4, mode == 92 ? 3 : 1, SIXEL_PIXELFORMAT_RGB888)
        == SIXEL_OK);
    CONTEXT_REQUIRE(sixel_repair_config_init(&config, sizeof(config))
        == SIXEL_OK);
    config.stop_distance_auto = 0;
    config.selection_fraction = 1;
    config.noise_strength = 0;
    config.max_colors = 4;
    CONTEXT_REQUIRE(sixel_repair_context_new(&context, encoder, &config, NULL)
        == SIXEL_OK);
    CONTEXT_REQUIRE(sixel_repair_context_bind(context, source, mode == 92,
        mode == 92, 1) == SIXEL_OK);
    if (mode == 92) {
        CONTEXT_REQUIRE(sixel_output_new(&output, context_test_write, NULL,
            NULL) == SIXEL_OK);
        CONTEXT_REQUIRE(context_test_step(context, output, 1, 4096, &result)
            == SIXEL_OK && result.audit.outcome == SIXEL_REPAIR_FRAME_EMITTED &&
            result.painted == 3 && encoder->sixdelta_origin_x == 0 &&
            encoder->sixdelta_origin_y == 0);
        for (i = 0; i < 18; ++i) {
            memcpy(expected, history + i * 3, 3);
            if (i == 7 || i == 8 || i == 10) {
                memcpy(expected, rgba + (i - 7) * 4, 3);
            }
            CONTEXT_REQUIRE(memcmp(encoder->accumulation_pixels + i * 3,
                expected, 3) == 0 && encoder->accumulation_valid_mask[i]);
        }
    }
#if SIXEL_ENABLE_THREADS
    else {
        CONTEXT_REQUIRE(sixel_output_new(&ordinary, context_test_write, NULL,
            NULL) == SIXEL_OK);
        probe.encoder = encoder;
        probe.context = context;
        probe.frame = source;
        probe.ordinary = ordinary;
        probe.after_admission = mode == 94;
        CONTEXT_REQUIRE(sixel_output_new(&output, context_order_write, &probe,
            NULL) == SIXEL_OK);
        CONTEXT_REQUIRE(sixel_encoder_set_cancel_callback(encoder,
            context_order_cancel, &probe) == SIXEL_OK);
        /* One generous call reaches the exact admission boundary before
         * the callback registers a waiting ordinary update. */
        memset(&work, 0, sizeof(work));
        work.pixel_limit = work.distance_limit = 100000;
        CONTEXT_REQUIRE(sixel_repair_context_step(context, &work, 4096,
            output, &result) == SIXEL_OK);
        CONTEXT_REQUIRE(probe.launched && probe.registered);
        sixel_thread_join(&probe.worker);
        probe.launched = 0;
        CONTEXT_REQUIRE(probe.status == SIXEL_OK &&
            probe.emitting == probe.after_admission);
        if (mode == 93) {
            CONTEXT_REQUIRE(result.audit.outcome == SIXEL_REPAIR_YIELDED &&
                probe.calls == 0 && probe.ordinary_painted == 4);
        } else {
            CONTEXT_REQUIRE(result.audit.outcome ==
                SIXEL_REPAIR_FRAME_EMITTED &&
                result.painted == 4 && probe.calls == 1 &&
                probe.ordinary_painted == 0);
        }
        CONTEXT_REQUIRE(context_test_step(context, output, 1, 4096, &result)
            == SIXEL_OK && result.audit.outcome == SIXEL_REPAIR_NEEDS_SOURCE);
    }
#endif
    ok = 1;
end:
#if SIXEL_ENABLE_THREADS
    if (probe.launched) {
        sixel_thread_join(&probe.worker);
    }
#endif
    (void)sixel_repair_context_destroy(context);
    sixel_frame_unref(source);
    sixel_output_unref(output);
    sixel_output_unref(ordinary);
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* Expire playback's deadline only after the final commit record is ready,
 * exercising the checkpoint immediately before output admission. */
static int
context_test_deadline(void *priv)
{
    sixel_repair_context_t *context;

    context = priv;
    return context->commit_count != 0 &&
        context->packet_cursor == context->count;
}

int
test_progressive_public_case(int mode)
{
    sixel_encoder_t *encoder;
    sixel_progressive_t *context;
    sixel_progressive_config_t config;
    sixel_progressive_budget_t budget;
    sixel_progressive_stats_t stats;
    struct {
        sixel_progressive_stats_t stats;
        unsigned char tail[16];
    } future;
    sixel_frame_t *source;
    sixel_output_t *output;
    context_step_sink_t sink;
    unsigned char target[6] = { 255, 0, 0, 0, 255, 0 };
    unsigned char black[6] = { 0 };
    uint64_t visits;
    uint64_t distances;
    uint64_t revision;
    FILE *file;
    int saved_fd;
    size_t n;
    int ok;

    encoder = NULL;
    context = NULL;
    source = NULL;
    output = NULL;
    file = NULL;
    saved_fd = -1;
    ok = 0;
    memset(&sink, 0, sizeof(sink));
    memset(&budget, 0, sizeof(budget));
    budget.struct_size = sizeof(budget);
    budget.max_packet_bytes = 4096;
    budget.max_pixel_visits = budget.max_distance_evaluations = 1;
    memset(&stats, 0, sizeof(stats));
    stats.struct_size = sizeof(stats);
    CONTEXT_REQUIRE(sixel_encoder_new(&encoder, NULL) == SIXEL_OK);
    CONTEXT_REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_UPDATE_POLICY,
        "delta") == SIXEL_OK);
    CONTEXT_REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_GPU_POLICY,
        "off") == SIXEL_OK);
    CONTEXT_REQUIRE(sixel_encoder_setopt(encoder,
        SIXEL_OPTFLAG_WORKING_COLORSPACE, "gamma") == SIXEL_OK);
    CONTEXT_REQUIRE(sixel_encoder_set_accumulation_buffer(encoder, black,
        2, 1, SIXEL_PIXELFORMAT_RGB888) == SIXEL_OK);
    CONTEXT_REQUIRE(sixel_progressive_config_init(&config, sizeof(config))
        == SIXEL_OK);
    config.stop_distance_auto = 0;
    config.selection_fraction = 1;
    config.noise_strength = 0;
    config.max_colors = 2;
    CONTEXT_REQUIRE(sixel_progressive_new(&context, encoder, &config, NULL)
        == SIXEL_OK);
    source = context_test_frame(target, 2, 1, SIXEL_PIXELFORMAT_RGB888);
    CONTEXT_REQUIRE(source != NULL);
    CONTEXT_REQUIRE(sixel_progressive_bind_frame(context, source, 0, 0, 1)
        == SIXEL_OK);
    CONTEXT_REQUIRE(sixel_output_new(&output, context_step_write, &sink,
        NULL) == SIXEL_OK);
    revision = encoder->history_revision;
    if (mode == 96) {
        /* Inspect the selected default destination, not stdout. This stream
         * is required evidence for the NULL-output branch's byte contract. */
        file = tmpfile();
        CONTEXT_REQUIRE(file != NULL);
        saved_fd = encoder->outfd;
        encoder->outfd = fileno(file);
        CONTEXT_REQUIRE(encoder->outfd >= 0);
    } else if (mode == 97) {
        memset(&future, 0xa5, sizeof(future));
        future.stats.struct_size = sizeof(stats) - 1;
        CONTEXT_REQUIRE(sixel_progressive_step(context, &budget, output,
            &future.stats) == SIXEL_BAD_ARGUMENT);
        CONTEXT_REQUIRE(future.stats.struct_size == sizeof(stats) - 1 &&
            future.tail[0] == 0xa5);
        budget.struct_size = sizeof(budget) - 1;
        future.stats.struct_size = sizeof(future);
        CONTEXT_REQUIRE(sixel_progressive_step(context, &budget, output,
            &future.stats) == SIXEL_BAD_ARGUMENT &&
            future.stats.outcome == SIXEL_PROGRESSIVE_NONE &&
            future.tail[0] == 0xa5);
        budget.struct_size = sizeof(budget);
        budget.max_packet_bytes = 0;
        CONTEXT_REQUIRE(sixel_progressive_step(context, &budget, output,
            &future.stats) == SIXEL_BAD_ARGUMENT &&
            future.stats.state_snapshot_valid == 0 &&
            future.tail[15] == 0xa5);
        budget.max_packet_bytes = 4096;
    } else if (mode == 98) {
        context->total_pixel_visits = UINT64_MAX;
        CONTEXT_REQUIRE(sixel_progressive_step(context, &budget, output,
            &stats) == SIXEL_BAD_INTEGER_OVERFLOW && sink.calls == 0 &&
            encoder->history_revision == revision &&
            context->total_pixel_visits == UINT64_MAX);
        CONTEXT_REQUIRE(sixel_progressive_bind_frame(context, source, 0, 0, 2)
            == SIXEL_OK && context->total_pixel_visits == 0);
    } else if (mode == 99) {
        sixel_output_set_skip_dcs_envelope(output, 1);
    }
    if (mode == 110) {
        budget.max_pixel_visits = budget.max_distance_evaluations = 100000;
        context->yield_requested = context_test_deadline;
        context->yield_priv = context;
        CONTEXT_REQUIRE(sixel_progressive_step(context, &budget, output,
            &stats) == SIXEL_OK && stats.outcome == SIXEL_PROGRESSIVE_YIELDED
            && stats.yield_reason == SIXEL_PROGRESSIVE_YIELD_WORK_BUDGET);
        CONTEXT_REQUIRE(sink.calls == 0 &&
            encoder->history_revision == revision &&
            memcmp(encoder->accumulation_pixels, black, sizeof(black)) == 0);
        context->yield_requested = NULL;
        CONTEXT_REQUIRE(sixel_progressive_step(context, &budget, output,
            &stats) == SIXEL_OK &&
            stats.outcome == SIXEL_PROGRESSIVE_FRAME_EMITTED &&
            sink.calls == 1 && stats.painted_pixels == 2 &&
            memcmp(encoder->accumulation_pixels, target, sizeof(target)) == 0);
        ok = 1;
        goto end;
    }
    visits = distances = 0;
    for (n = 0; n < 10000; ++n) {
        CONTEXT_REQUIRE(sixel_progressive_step(context, &budget,
            mode == 96 ? NULL : output, &stats) == SIXEL_OK);
        visits += stats.pixel_visits;
        distances += stats.distance_evaluations;
        CONTEXT_REQUIRE(stats.struct_size == sizeof(stats) &&
            stats.state_snapshot_valid &&
            stats.total_pixel_visits == visits &&
            stats.total_distance_evaluations == distances &&
            stats.pixel_visits <= 1 && stats.distance_evaluations <= 1 &&
            stats.backend == SIXEL_PROGRESSIVE_BACKEND_CPU &&
            stats.working_colorspace == SIXEL_COLORSPACE_GAMMA);
        if (stats.outcome != SIXEL_PROGRESSIVE_YIELDED) {
            break;
        }
        CONTEXT_REQUIRE(stats.yield_reason ==
            SIXEL_PROGRESSIVE_YIELD_WORK_BUDGET &&
            stats.packet_bytes == 0 && stats.painted_pixels == 0);
    }
    if (mode == 99) {
        CONTEXT_REQUIRE(stats.outcome == SIXEL_PROGRESSIVE_UNSUPPORTED &&
            encoder->history_revision == revision && sink.calls == 0);
        ok = 1;
        goto end;
    }
    CONTEXT_REQUIRE(stats.outcome == SIXEL_PROGRESSIVE_FRAME_EMITTED &&
        stats.painted_pixels == 2 && stats.packet_bytes > 0 &&
        stats.audit_complete == 0 && stats.selected_count == 2 &&
        stats.constructed_colors == 2 && stats.transmitted_colors == 2 &&
        stats.stage == SIXEL_PROGRESSIVE_STAGE_COMMIT &&
        stats.history_revision == revision + 1 &&
        stats.sampled_cost_before > stats.sampled_cost_after &&
        memcmp(encoder->accumulation_pixels, target, sizeof(target)) == 0);
    if (mode == 96) {
        rewind(file);
        sink.size = fread(sink.bytes, 1, sizeof(sink.bytes), file);
        CONTEXT_REQUIRE(sink.size == stats.packet_bytes &&
            memcmp(sink.bytes, "\033P0;1q", 6) == 0 &&
            memcmp(sink.bytes + sink.size - 2, "\033\\", 2) == 0);
    } else {
        CONTEXT_REQUIRE(sink.size == stats.packet_bytes && sink.calls == 1);
    }
    budget.max_pixel_visits = budget.max_distance_evaluations = 10000;
    CONTEXT_REQUIRE(sixel_progressive_step(context, &budget, output, &stats)
        == SIXEL_OK && stats.outcome == SIXEL_PROGRESSIVE_CONVERGED &&
        stats.audit_complete && stats.unresolved_count == 0 &&
        stats.maximum_distance == 0 && stats.sum_squared_error == 0 &&
        stats.packet_bytes == 0 && stats.painted_pixels == 0);
    CONTEXT_REQUIRE(sixel_encoder_setopt(encoder,
        SIXEL_OPTFLAG_WORKING_COLORSPACE, "linear") == SIXEL_OK);
    CONTEXT_REQUIRE(sixel_progressive_step(context, &budget, output, &stats)
        == SIXEL_OK && stats.outcome == SIXEL_PROGRESSIVE_NEEDS_SOURCE &&
        stats.state_snapshot_valid && !stats.audit_complete);
    ok = 1;
end:
    if (saved_fd >= 0) {
        encoder->outfd = saved_fd;
    }
    if (file != NULL) {
        fclose(file);
    }
    sixel_progressive_unref(context);
    sixel_frame_unref(source);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
test_progressive_gpu_context_case(int mode)
{
    sixel_encoder_t *encoder;
    sixel_progressive_t *context;
    sixel_frame_t *source;
    sixel_frame_t *update;
    sixel_output_t *output;
    sixel_output_t *ordinary;
    sixel_repair_gpu_t *probe;
    sixel_repair_gpu_lookup_t lookup;
    sixel_repair_gpu_match_t match;
    sixel_progressive_config_t config;
    sixel_progressive_budget_t budget;
    sixel_progressive_stats_t stats;
    context_step_sink_t sink;
    float query[3] = { 0 };
    unsigned char targets[2][6] = {
        { 255, 0, 0, 0, 255, 0 }, { 0, 0, 255, 255, 255, 255 }
    };
    unsigned char black[5 * 4 * 3] = { 0 };
    unsigned char *decoded;
    uint64_t revision;
    size_t pass;
    size_t calls;
    size_t i;
    size_t passes;
    unsigned int flags;
    int width;
    int height;
    int cancel;
    int interrupted;
    int expected_backend;
    int ok;
    SIXELSTATUS status;

    encoder = NULL;
    context = NULL;
    source = update = NULL;
    output = ordinary = NULL;
    probe = NULL;
    decoded = NULL;
    cancel = interrupted = ok = 0;
    lookup.queries = lookup.palette = query;
    lookup.count = lookup.colors = 1;
    lookup.policy = SIXEL_GPU_POLICY_FORCE;
    CONTEXT_REQUIRE(sixel_repair_gpu_new(&probe, NULL) == SIXEL_OK);
    status = sixel_repair_gpu_lookup(probe, &lookup, &match);
    if (status == SIXEL_FEATURE_ERROR) {
        ok = 77;
        goto end;
    }
    CONTEXT_REQUIRE(status == SIXEL_OK);
    CONTEXT_REQUIRE(sixel_encoder_new(&encoder, NULL) == SIXEL_OK);
    CONTEXT_REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_UPDATE_POLICY,
        "delta") == SIXEL_OK);
    CONTEXT_REQUIRE(sixel_encoder_setopt(encoder,
        SIXEL_OPTFLAG_WORKING_COLORSPACE, "gamma") == SIXEL_OK);
    CONTEXT_REQUIRE(sixel_encoder_set_cancel_callback(encoder,
        context_test_cancel, &cancel) == SIXEL_OK);
    CONTEXT_REQUIRE(sixel_progressive_config_init(&config, sizeof(config))
        == SIXEL_OK);
    config.stop_distance_auto = 0;
    config.noise_strength = 0;
    config.selection_fraction = 1;
    config.max_colors = 2;
    if (mode == 190) {
        config.gpu_policy = SIXEL_GPU_POLICY_FORCE;
        update = context_test_frame(black, 5, 4, SIXEL_PIXELFORMAT_RGB888);
        CONTEXT_REQUIRE(update != NULL && sixel_output_new(&ordinary,
            context_test_write, NULL, NULL) == SIXEL_OK);
    }
    CONTEXT_REQUIRE(sixel_progressive_new(&context, encoder, &config, NULL)
        == SIXEL_OK);
    CONTEXT_REQUIRE(sixel_output_new(&output, context_step_write, &sink,
        NULL) == SIXEL_OK);
    memset(&budget, 0, sizeof(budget));
    budget.struct_size = sizeof(budget);
    budget.max_packet_bytes = 4096;
    memset(&stats, 0, sizeof(stats));
    stats.struct_size = sizeof(stats);
    for (pass = 0; pass < ((mode == 177 || mode == 178 || mode == 190)
            ? 2u : 1u); ++pass) {
        memset(&sink, 0, sizeof(sink));
        sink.mode = mode == 179 ? 82 : 0;
        CONTEXT_REQUIRE(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_GPU_POLICY,
            mode == 190 ? "off" :
            mode == 178 ? (pass ? "auto:P2" : "auto:P3") :
            pass && mode != 190 ? "off" : "force") == SIXEL_OK);
        CONTEXT_REQUIRE(sixel_encoder_set_accumulation_buffer(encoder, black,
            5, 4, SIXEL_PIXELFORMAT_RGB888) == SIXEL_OK);
        sixel_frame_unref(source);
        source = context_test_frame(targets[pass], 2, 1,
            SIXEL_PIXELFORMAT_RGB888);
        CONTEXT_REQUIRE(source != NULL);
        CONTEXT_REQUIRE(sixel_progressive_bind_frame(context, source, 1, 1,
            pass + 1) == SIXEL_OK && context->gpu == NULL);
        revision = encoder->history_revision;
        budget.max_pixel_visits = budget.max_distance_evaluations =
            mode == 176 || mode == 180 || mode == 190 ? 1 : 100000;
        expected_backend = (mode == 177 && pass) ||
            (mode == 178 && !pass) ? SIXEL_PROGRESSIVE_BACKEND_CPU
            : SIXEL_PROGRESSIVE_BACKEND_METAL;
        for (calls = 0; calls < 10000; ++calls) {
            status = sixel_progressive_step(context, &budget, output, &stats);
            if (mode == 179 && SIXEL_FAILED(status)) break;
            CONTEXT_REQUIRE(status == SIXEL_OK && stats.pixel_visits <=
                budget.max_pixel_visits && stats.distance_evaluations <=
                budget.max_distance_evaluations);
            if (stats.outcome != SIXEL_PROGRESSIVE_YIELDED) break;
            CONTEXT_REQUIRE(sink.calls == 0 &&
                encoder->history_revision == revision &&
                memcmp(encoder->accumulation_pixels, black, sizeof(black))
                    == 0);
            if (mode == 190 && !pass && context->selection != NULL &&
                    context->selection->gpu_used) {
                CONTEXT_REQUIRE(!context->application.gpu_used &&
                    stats.backend == SIXEL_PROGRESSIVE_BACKEND_METAL);
                CONTEXT_REQUIRE(sixel_encoder_encode_frame(encoder,
                    update, ordinary) == SIXEL_OK);
                CONTEXT_REQUIRE(sixel_progressive_step(context, &budget,
                    output, &stats) == SIXEL_OK && stats.outcome ==
                        SIXEL_PROGRESSIVE_NEEDS_SOURCE && sink.calls == 0);
                interrupted = 1;
                break;
            }
            if (mode == 180 && stats.backend ==
                    SIXEL_PROGRESSIVE_BACKEND_METAL &&
                    !(interrupted & (context->application.gpu_used
                        ? 2 : 1))) {
                cancel = 1;
                CONTEXT_REQUIRE(sixel_progressive_step(context, &budget,
                    output, &stats) == SIXEL_INTERRUPTED && sink.calls == 0 &&
                    encoder->history_revision == revision);
                cancel = 0;
                interrupted |= context->application.gpu_used ? 2 : 1;
            }
        }
        CONTEXT_REQUIRE(calls < 10000);
        if (mode == 190 && !pass) {
            CONTEXT_REQUIRE(interrupted && sink.calls == 0);
            continue;
        }
        if (mode == 179) {
            CONTEXT_REQUIRE(SIXEL_FAILED(status) && sink.calls == 1 &&
                !encoder->accumulation_valid &&
                stats.packet_bytes == 0 && stats.painted_pixels == 0);
            CONTEXT_REQUIRE(sixel_progressive_step(context, &budget, output,
                &stats) == SIXEL_OK &&
                stats.outcome == SIXEL_PROGRESSIVE_NEEDS_BASELINE);
            ok = 1;
            goto end;
        }
        CONTEXT_REQUIRE(stats.outcome == SIXEL_PROGRESSIVE_FRAME_EMITTED &&
            stats.backend == expected_backend && stats.painted_pixels == 2 &&
            sink.calls == 1 && stats.packet_bytes == sink.size &&
            encoder->history_revision == revision + 1);
        for (i = 0; i < 20; ++i) {
            CONTEXT_REQUIRE(memcmp(encoder->accumulation_pixels + i * 3,
                i == 6 || i == 7 ? targets[pass] + (i - 6) * 3 : black,
                3) == 0);
        }
        CONTEXT_REQUIRE(sixel_decode_sequence_mapped(sink.bytes, sink.size,
            NULL, &decoded, &width, &height, &flags, &passes,
            encoder->allocator) == SIXEL_OK);
        CONTEXT_REQUIRE(width == 2 && height == 1 && passes == 1);
        for (i = 0; i < 2; ++i) {
            CONTEXT_REQUIRE(memcmp(decoded + i * 4, targets[pass] + i * 3,
                3) == 0 && decoded[i * 4 + 3] == 255);
        }
        free(decoded);
        decoded = NULL;
        budget.max_pixel_visits = budget.max_distance_evaluations = 100000;
        CONTEXT_REQUIRE(sixel_progressive_step(context, &budget, output,
            &stats) == SIXEL_OK &&
            stats.outcome == SIXEL_PROGRESSIVE_CONVERGED &&
            stats.backend == SIXEL_PROGRESSIVE_BACKEND_CPU &&
            stats.audit_complete && stats.sum_squared_error == 0 &&
            sink.calls == 1);
    }
    CONTEXT_REQUIRE(mode != 180 || interrupted == 3);
    ok = 1;
end:
    free(decoded);
    sixel_progressive_unref(context);
    sixel_frame_unref(source);
    sixel_frame_unref(update);
    sixel_output_unref(ordinary);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    sixel_repair_gpu_free(probe);
    return ok == 77 ? 77 : ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

#undef CONTEXT_REQUIRE
