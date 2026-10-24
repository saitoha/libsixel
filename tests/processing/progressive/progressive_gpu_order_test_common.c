/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder.h"
#include "src/progressive.h"
#include "src/sixel_decode_pixels.h"

#if SIXEL_ENABLE_THREADS
typedef struct gpu_order_probe {
    sixel_encoder_t *encoder;
    sixel_progressive_t *context;
    sixel_frame_t *frame;
    sixel_output_t *ordinary;
    sixel_thread_t worker;
    unsigned char bytes[4096];
    unsigned char ordinary_bytes[4096];
    size_t size;
    size_t ordinary_size;
    size_t calls;
    size_t ordinary_painted;
    int mode;
    int launched;
    int registered;
    int emitting;
    int scanned;
    int applied;
    int binding_alive;
    SIXELSTATUS status;
} gpu_order_probe_t;

/* All context allocations occur under the encoder gate. The main thread
 * reads this counter only before launching or after joining the worker. */
static size_t gpu_order_live;

static void *
gpu_order_malloc(size_t size)
{
    void *data;

    data = malloc(size);
    gpu_order_live += data != NULL;
    return data;
}

static void *
gpu_order_calloc(size_t count, size_t size)
{
    void *data;

    data = calloc(count, size);
    gpu_order_live += data != NULL;
    return data;
}

static void *
gpu_order_realloc(void *previous, size_t size)
{
    void *data;
    int fresh;

    fresh = previous == NULL;
    data = realloc(previous, size);
    gpu_order_live += data != NULL && fresh;
    return data;
}

static void
gpu_order_free(void *data)
{
    gpu_order_live -= data != NULL;
    free(data);
}

static sixel_frame_t *
gpu_order_frame(unsigned char const *rgb)
{
    sixel_frame_t *frame;
    unsigned char *owned;

    frame = NULL;
    owned = malloc(12);
    if (owned == NULL) return NULL;
    memcpy(owned, rgb, 12);
    if (sixel_frame_new(&frame, NULL) != SIXEL_OK ||
            sixel_frame_init(frame, owned, 4, 1, SIXEL_PIXELFORMAT_RGB888,
                NULL, 0) != SIXEL_OK) {
        free(owned);
        sixel_frame_unref(frame);
        return NULL;
    }
    return frame;
}

static int
gpu_order_worker(void *priv)
{
    gpu_order_probe_t *probe;

    probe = priv;
    if (probe->mode == 196) {
        probe->status = sixel_progressive_bind_frame(probe->context,
            probe->frame, 0, 0, 2);
    } else if (probe->mode == 197) {
        sixel_progressive_unref(probe->context);
        probe->context = NULL;
        probe->status = SIXEL_OK;
    } else {
        probe->status = sixel_encoder_encode_frame(probe->encoder,
            probe->frame, probe->ordinary);
        probe->ordinary_painted =
            sixel_encoder_get_6delta_painted_pixels(probe->encoder);
    }
    return 0;
}

static void
gpu_order_register(gpu_order_probe_t *probe)
{
    sixel_repair_gpu_t *cache;
    sixel_frame_t *source;
    float *retained;

    if (probe->launched) return;
    cache = probe->context->gpu;
    source = probe->context->source;
    retained = probe->context->gpu_retained;
    probe->scanned = probe->context->selection != NULL &&
        probe->context->selection->gpu_used;
    probe->applied = probe->context->application.gpu_used;
    probe->launched = sixel_thread_create(&probe->worker,
        gpu_order_worker, probe) == 0;
    if (!probe->launched) return;
    /* Wait for actual gate registration, never for an estimated GPU delay.
     * The caller still owns the gate, so the worker cannot release buffers. */
    sixel_mutex_lock(&probe->encoder->operation_mutex);
    while (!probe->encoder->operation_waiters) {
        sixel_cond_wait(&probe->encoder->operation_cond,
            &probe->encoder->operation_mutex);
    }
    probe->emitting = probe->encoder->operation_emitting;
    probe->registered = 1;
    probe->binding_alive = cache != NULL && source != NULL &&
        retained != NULL && probe->context->gpu == cache &&
        probe->context->source == source &&
        probe->context->gpu_retained == retained &&
        probe->encoder->progressive_context == probe->context;
    sixel_mutex_unlock(&probe->encoder->operation_mutex);
}

static int
gpu_order_cancel(void *priv)
{
    gpu_order_probe_t *probe;

    probe = priv;
    if (probe->launched) return 0;
    /* These callbacks run at the first owner checkpoint after a synchronous
     * Metal batch, or after packet preparation but before output admission. */
    if ((probe->mode == 192 && probe->context->selection != NULL &&
            probe->context->selection->gpu_used) ||
            ((probe->mode == 193 || probe->mode == 196) &&
             probe->context->application.gpu_used) ||
            (probe->mode == 194 && probe->context->commit_count == 4)) {
        gpu_order_register(probe);
    }
    return 0;
}

static int
gpu_order_write(char *data, int size, void *priv)
{
    gpu_order_probe_t *probe;

    probe = priv;
    if (size < 0 || (size_t)size > sizeof(probe->bytes) - probe->size) {
        return -1;
    }
    memcpy(probe->bytes + probe->size, data, (size_t)size);
    probe->size += (size_t)size;
    ++probe->calls;
    if (probe->mode == 195 || probe->mode == 197) {
        gpu_order_register(probe);
    }
    return 0;
}

static int
gpu_order_ordinary_write(char *data, int size, void *priv)
{
    gpu_order_probe_t *probe;

    probe = priv;
    if (size < 0 || (size_t)size >
            sizeof(probe->ordinary_bytes) - probe->ordinary_size) {
        return -1;
    }
    memcpy(probe->ordinary_bytes + probe->ordinary_size, data, (size_t)size);
    probe->ordinary_size += (size_t)size;
    return 0;
}
#endif

#define GPU_ORDER_REQUIRE(value) do { \
    if (!(value)) { \
        fprintf(stderr, "GPU order case %d, line %d\n", mode, __LINE__); \
        goto end; \
    } \
} while (0)

int
test_progressive_gpu_order_case(int mode)
{
#if !SIXEL_ENABLE_THREADS
    (void)mode;
    return 77;
#else
    sixel_allocator_t *allocator;
    sixel_encoder_t *encoder;
    sixel_frame_t *source;
    sixel_frame_t *replacement;
    sixel_output_t *output;
    sixel_repair_gpu_t *gpu;
    sixel_repair_gpu_lookup_t lookup;
    sixel_repair_gpu_match_t match;
    sixel_progressive_config_t config;
    sixel_progressive_budget_t budget;
    sixel_progressive_stats_t stats;
    gpu_order_probe_t probe;
    float query[3] = { 0 };
    unsigned char target[12] = { 255, 0, 0, 0, 255, 0,
                                0, 0, 255, 255, 255, 255 };
    unsigned char next[12] = { 255, 255, 0, 0, 255, 255,
                              255, 0, 255, 255, 255, 255 };
    unsigned char black[12] = { 0 };
    unsigned char *decoded;
    unsigned char const *expected;
    size_t passes;
    size_t i;
    unsigned int flags;
    int width;
    int height;
    int after;
    int ok;
    SIXELSTATUS status;

    allocator = NULL;
    encoder = NULL;
    source = replacement = NULL;
    output = NULL;
    gpu = NULL;
    decoded = NULL;
    ok = 0;
    after = mode == 195 || mode == 197;
    memset(&probe, 0, sizeof(probe));
    probe.mode = mode;
    gpu_order_live = 0;
    lookup.queries = lookup.palette = query;
    lookup.count = lookup.colors = 1;
    lookup.policy = SIXEL_GPU_POLICY_FORCE;
    GPU_ORDER_REQUIRE(sixel_repair_gpu_new(&gpu, NULL) == SIXEL_OK);
    status = sixel_repair_gpu_lookup(gpu, &lookup, &match);
    if (status == SIXEL_FEATURE_ERROR) {
        ok = 77;
        goto end;
    }
    GPU_ORDER_REQUIRE(status == SIXEL_OK);
    GPU_ORDER_REQUIRE(sixel_allocator_new(&allocator, gpu_order_malloc,
        gpu_order_calloc, gpu_order_realloc, gpu_order_free) == SIXEL_OK);
    GPU_ORDER_REQUIRE(sixel_encoder_new(&encoder, NULL) == SIXEL_OK);
    probe.encoder = encoder;
    GPU_ORDER_REQUIRE(sixel_encoder_setopt(encoder,
        SIXEL_OPTFLAG_UPDATE_POLICY, "delta") == SIXEL_OK);
    GPU_ORDER_REQUIRE(sixel_encoder_setopt(encoder,
        SIXEL_OPTFLAG_GPU_POLICY, "off") == SIXEL_OK);
    GPU_ORDER_REQUIRE(sixel_encoder_setopt(encoder,
        SIXEL_OPTFLAG_WORKING_COLORSPACE, "gamma") == SIXEL_OK);
    GPU_ORDER_REQUIRE(sixel_encoder_setopt(encoder,
        SIXEL_OPTFLAG_THREADING_POLICY, "balanced:B1") == SIXEL_OK);
    source = gpu_order_frame(target);
    replacement = gpu_order_frame(next);
    GPU_ORDER_REQUIRE(source != NULL && replacement != NULL);
    probe.frame = mode == 196 ? replacement : source;
    GPU_ORDER_REQUIRE(sixel_encoder_set_accumulation_buffer(encoder, black,
        4, 1, SIXEL_PIXELFORMAT_RGB888) == SIXEL_OK);
    GPU_ORDER_REQUIRE(sixel_progressive_config_init(&config, sizeof(config))
        == SIXEL_OK);
    config.stop_distance_auto = 0;
    config.selection_fraction = 1;
    config.noise_strength = 0;
    config.max_colors = 4;
    config.gpu_policy = SIXEL_GPU_POLICY_FORCE;
    GPU_ORDER_REQUIRE(sixel_progressive_new(&probe.context, encoder,
        &config, allocator) == SIXEL_OK);
    GPU_ORDER_REQUIRE(sixel_progressive_bind_frame(probe.context, source,
        0, 0, 1) == SIXEL_OK);
    GPU_ORDER_REQUIRE(sixel_output_new(&probe.ordinary,
        gpu_order_ordinary_write, &probe, NULL) == SIXEL_OK);
    GPU_ORDER_REQUIRE(sixel_output_new(&output, gpu_order_write, &probe,
        NULL) == SIXEL_OK);
    GPU_ORDER_REQUIRE(sixel_encoder_set_cancel_callback(encoder,
        gpu_order_cancel, &probe) == SIXEL_OK);
    memset(&budget, 0, sizeof(budget));
    budget.struct_size = sizeof(budget);
    budget.max_pixel_visits = budget.max_distance_evaluations = 100000;
    budget.max_packet_bytes = sizeof(probe.bytes);
    memset(&stats, 0, sizeof(stats));
    stats.struct_size = sizeof(stats);
    status = sixel_progressive_step(probe.context, &budget, output, &stats);
    GPU_ORDER_REQUIRE(probe.launched && probe.registered);
    sixel_thread_join(&probe.worker);
    probe.launched = 0;
    GPU_ORDER_REQUIRE(status == SIXEL_OK && probe.status == SIXEL_OK &&
        probe.binding_alive && probe.emitting == after && probe.scanned &&
        probe.applied == (mode != 192) &&
        stats.backend == SIXEL_PROGRESSIVE_BACKEND_METAL &&
        stats.source_generation == 1);
    GPU_ORDER_REQUIRE(sixel_encoder_set_cancel_callback(encoder, NULL, NULL)
        == SIXEL_OK);
    if (after) {
        GPU_ORDER_REQUIRE(stats.outcome == SIXEL_PROGRESSIVE_FRAME_EMITTED &&
            stats.painted_pixels == 4 && probe.calls == 1 &&
            stats.packet_bytes == probe.size &&
            probe.ordinary_painted == 0);
    } else {
        GPU_ORDER_REQUIRE(stats.outcome == SIXEL_PROGRESSIVE_YIELDED &&
            stats.yield_reason == SIXEL_PROGRESSIVE_YIELD_UPDATE_PENDING &&
            probe.calls == 0 && stats.painted_pixels == 0);
    }
    if (mode == 196) {
        /* Rebinding must discard both completed device results and host
         * work before the replacement source can use the same context. */
        GPU_ORDER_REQUIRE(probe.context->generation == 2 &&
            probe.context->gpu == NULL &&
            probe.context->gpu_retained == NULL &&
            memcmp(encoder->accumulation_pixels, black, sizeof(black)) == 0);
        GPU_ORDER_REQUIRE(sixel_progressive_step(probe.context, &budget,
            output, &stats) == SIXEL_OK &&
            stats.outcome == SIXEL_PROGRESSIVE_FRAME_EMITTED &&
            stats.backend == SIXEL_PROGRESSIVE_BACKEND_METAL &&
            stats.source_generation == 2 && stats.painted_pixels == 4 &&
            probe.calls == 1);
    } else if (mode == 197) {
        /* The public step returns its owned snapshot even if unref has
         * already freed the context as soon as the operation gate opens. */
        GPU_ORDER_REQUIRE(probe.context == NULL &&
            encoder->progressive_context == NULL && gpu_order_live == 1);
    } else {
        GPU_ORDER_REQUIRE(probe.ordinary_painted == (after ? 0u : 4u));
        GPU_ORDER_REQUIRE(sixel_progressive_step(probe.context, &budget,
            output, &stats) == SIXEL_OK &&
            stats.outcome == SIXEL_PROGRESSIVE_NEEDS_SOURCE);
    }
    expected = mode == 196 ? next : target;
    if (after || mode == 196) {
        /* GPU repair preserves these endpoints exactly in gamma and the
         * wire palette, so stale source/cache colors cannot pass. */
        GPU_ORDER_REQUIRE(memcmp(encoder->accumulation_pixels, expected, 12)
            == 0);
        GPU_ORDER_REQUIRE(sixel_decode_sequence_mapped(probe.bytes,
            probe.size, NULL, &decoded, &width, &height, &flags, &passes,
            encoder->allocator) == SIXEL_OK);
    } else {
        /* Ordinary quantization may approximate the source. Its decoded
         * packet, rather than the source, must match committed history. */
        expected = encoder->accumulation_pixels;
        GPU_ORDER_REQUIRE(sixel_decode_sequence_mapped(probe.ordinary_bytes,
            probe.ordinary_size, NULL, &decoded, &width, &height, &flags,
            &passes, encoder->allocator) == SIXEL_OK);
    }
    GPU_ORDER_REQUIRE(width == 4 && height == 1 && passes == 1);
    for (i = 0; i < 4; ++i) {
        GPU_ORDER_REQUIRE(memcmp(decoded + i * 4, expected + i * 3,
            3) == 0 && decoded[i * 4 + 3] == 255);
    }
    ok = 1;
end:
    if (probe.launched) sixel_thread_join(&probe.worker);
    if (encoder != NULL) {
        (void)sixel_encoder_set_cancel_callback(encoder, NULL, NULL);
    }
    free(decoded);
    sixel_progressive_unref(probe.context);
    sixel_frame_unref(source);
    sixel_frame_unref(replacement);
    sixel_output_unref(output);
    sixel_output_unref(probe.ordinary);
    sixel_encoder_unref(encoder);
    sixel_allocator_unref(allocator);
    sixel_repair_gpu_free(gpu);
    if (ok == 77) return 77;
    return ok && gpu_order_live == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
#endif
}

#undef GPU_ORDER_REQUIRE
