/* SPDX-License-Identifier: MIT */
/* Each wrapper selects one changed binding or lifetime observation. Byte
 * comparisons use a fresh core as the cache-independent reference; they do
 * not claim to independently validate the SIXEL encoding algorithm. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdlib.h>
#include <string.h>
#include <6cells.h>
#include "src/encoder-core.h"
#include "src/encode-result.h"
#include "src/palette.h"
#include "src/threading.h"
#include "src/sixel_atomic.h"

#if SIXEL_ENABLE_THREADS
typedef struct reuse_sink {
    unsigned char bytes[16384];
    size_t used;
    int reject;
} reuse_sink_t;

typedef struct reuse_trace {
    sixel_timeline_logger_vtbl_t const *vtbl;
    int cold;
    int warm;
    int rendezvous;
    int arrived;
    sixel_mutex_t mutex;
    sixel_cond_t cond;
} reuse_trace_t;

static sixel_atomic_u32_t reuse_live;
static sixel_atomic_u32_t reuse_allocations;
static int reuse_fail;

static void *
reuse_malloc(size_t size)
{
    void *p;

    (void)sixel_atomic_fetch_add_u32(&reuse_allocations, 1);
    if (reuse_fail) return NULL;
    p = malloc(size);
    if (p != NULL) (void)sixel_atomic_fetch_add_u32(&reuse_live, 1);
    return p;
}

static void *
reuse_calloc(size_t count, size_t size)
{
    void *p;

    (void)sixel_atomic_fetch_add_u32(&reuse_allocations, 1);
    if (reuse_fail) return NULL;
    p = calloc(count, size);
    if (p != NULL) (void)sixel_atomic_fetch_add_u32(&reuse_live, 1);
    return p;
}

static void *
reuse_realloc(void *old, size_t size)
{
    void *p;
    int was_null;

    (void)sixel_atomic_fetch_add_u32(&reuse_allocations, 1);
    if (reuse_fail) return NULL;
    was_null = old == NULL;
    p = realloc(old, size);
    if (p != NULL && was_null) {
        (void)sixel_atomic_fetch_add_u32(&reuse_live, 1);
    }
    return p;
}

static void
reuse_free(void *p)
{
    if (p != NULL) (void)sixel_atomic_fetch_sub_u32(&reuse_live, 1);
    free(p);
}

static SIXELSTATUS
reuse_log(sixel_timeline_logger_t *self, sixel_timeline_event_t const *event)
{
    reuse_trace_t *trace;

    trace = (reuse_trace_t *)self;
    if (strcmp(event->event, "context_begin") == 0) ++trace->cold;
    if (strcmp(event->event, "context_reuse") == 0) ++trace->warm;
    if (trace->rendezvous && strcmp(event->event, "worker_start") == 0) {
        /* Two bands and two workers: force both workspaces to initialize
         * before either worker can take the other's only job. */
        sixel_mutex_lock(&trace->mutex);
        ++trace->arrived;
        sixel_cond_broadcast(&trace->cond);
        while (trace->arrived < 2) {
            sixel_cond_wait(&trace->cond, &trace->mutex);
        }
        sixel_mutex_unlock(&trace->mutex);
    }
    return SIXEL_OK;
}

static sixel_timeline_logger_vtbl_t const reuse_log_vtbl = {
    NULL, NULL, reuse_log, NULL, NULL, NULL
};

static SIXELSTATUS
reuse_write(char const *bytes, size_t size, void *context)
{
    reuse_sink_t *sink;

    sink = context;
    if (sink->reject || size > sizeof(sink->bytes) - sink->used) {
        return SIXEL_RUNTIME_ERROR;
    }
    memcpy(sink->bytes + sink->used, bytes, size);
    sink->used += size;
    return SIXEL_OK;
}

static sixel_indexed_frame_t *
reuse_image(sixel_allocator_t *allocator, int observation, int second)
{
    sixel_wire_color_definition_t colors[4] = {
        {0, 2, {0, 10, 20}}, {1, 2, {30, 40, 50}},
        {2, 2, {60, 70, 80}}, {3, 2, {90, 90, 90}}
    };
    sixel_wire_palette_request_t wire;
    sixel_indexed_frame_description_t desc;
    sixel_wire_palette_t *palette;
    sixel_indexed_frame_t *image;
    int i;
    int width;
    int height;
    int count;

    palette = NULL;
    image = NULL;
    width = second && (observation == 4 || observation == 16) ? 17 : 16;
    height = second && observation == 5 ? 13 : 12;
    count = second && observation == 6 ? 4 : 3;
    if (second && observation == 1) colors[0].fields[0] = 100;
    memset(&wire, 0, sizeof(wire));
    wire.definitions = colors;
    wire.count = count;
    wire.color_definition = SIXEL_PALETTE_OUTPUT_LEGACY;
    wire.colorspace = SIXEL_COLORSPACE_GAMMA;
    if (sixel_wire_palette_new(&palette, &wire, allocator) != SIXEL_OK) {
        return NULL;
    }
    memset(&desc, 0, sizeof(desc));
    desc.width = desc.encoded_width = desc.plane_width = width;
    desc.height = desc.encoded_height = desc.plane_height = height;
    desc.palette = palette;
    desc.keycolor = -1;
    desc.transparent_background = observation == 2;
    if (second && observation == 11) {
        desc.encoded_x = 1;
        desc.width = width - 1;
    }
    if (sixel_indexed_frame_new(&image, &desc, allocator) == SIXEL_OK) {
        for (i = 0; i < desc.width * desc.height; ++i) {
            image->indexes[i] = (sixel_index_t)((i +
                (second && observation == 0 ? 1 : 0)) % count);
            image->skip[i] = second && observation == 2 && i % 7 == 0
                ? SIXEL_INDEXED_KEEP_APPROXIMATE : SIXEL_INDEXED_PAINT;
        }
    }
    sixel_wire_palette_unref(palette);
    return image;
}

static SIXELSTATUS
reuse_encode(sixel_encoder_core_t *core, sixel_indexed_frame_t *image,
    reuse_sink_t *sink, reuse_trace_t *trace, int observation, int second)
{
    sixel_encoder_core_prepare_request_t request;
    SIXELSTATUS status;
    int height;

    memset(&request, 0, sizeof(request));
    height = image->description.height;
    request.image = image;
    request.threads = second && observation == 7 ? 1 : 2;
    request.worker_capacity = request.threads;
    request.queue_bands = second && observation == 8 ? 1 : 2;
    request.options.emit_palette = observation != 14;
    request.options.ormode = second && observation == 9;
    request.options.encode_policy = second && observation == 10
        ? SIXEL_ENCODEPOLICY_SIZE : SIXEL_ENCODEPOLICY_FAST;
    request.sink.write = reuse_write;
    request.sink.context = sink;
    request.logger = (sixel_timeline_logger_t *)trace;
    status = core->vtbl->prepare(core, &request);
    if (status != SIXEL_OK) return status;
    if (sixel_indexed_frame_get_state(image) != SIXEL_INDEXED_COMPLETE) {
        status = sixel_indexed_frame_publish(image, height);
        if (status != SIXEL_OK) return status;
        status = sixel_indexed_frame_complete(image);
        if (status != SIXEL_OK) return status;
    }
    if (!second && observation == 12) {
        status = core->vtbl->consume(core, 6);
        if (status != SIXEL_OK) return status;
        return core->vtbl->abort(core);
    }
    status = core->vtbl->consume(core, height);
    if (status != SIXEL_OK) return status;
    return core->vtbl->finish(core);
}
#endif

int
core_reuse_observe(int observation)
{
#if SIXEL_ENABLE_THREADS
    sixel_allocator_t *allocator;
    sixel_encoder_core_t *core;
    sixel_encoder_core_t *fresh;
    sixel_indexed_frame_t *image;
    reuse_sink_t sinks[3];
    reuse_sink_t priming;
    reuse_trace_t trace;
    reuse_trace_t reference;
    void *object;
    SIXELSTATUS status;
    unsigned int before;
    unsigned int after;
    size_t previous_size;
    int ready;
    int ok;

    allocator = NULL;
    core = fresh = NULL;
    image = NULL;
    object = NULL;
    ready = ok = 0;
    reuse_live = reuse_allocations = 0;
    reuse_fail = 0;
    memset(sinks, 0, sizeof(sinks));
    memset(&priming, 0, sizeof(priming));
    memset(&trace, 0, sizeof(trace));
    memset(&reference, 0, sizeof(reference));
    trace.vtbl = reference.vtbl = &reuse_log_vtbl;
    if (sixel_mutex_init(&trace.mutex) != SIXEL_OK) goto end;
    ready = 1;
    if (sixel_cond_init(&trace.cond) != SIXEL_OK) goto end;
    ready = 2;
    trace.rendezvous = observation == 20;
    if (sixel_allocator_new(&allocator, reuse_malloc, reuse_calloc,
            reuse_realloc, reuse_free) != SIXEL_OK) goto end;
    if (sixel_encoder_core_factory_new(allocator, &object) != SIXEL_OK)
        goto end;
    core = object;
    before = sixel_atomic_fetch_add_u32(&reuse_live, 0);
    /* Failure/restart cases first park a successful context. The allocator
     * failure changes geometry to require new scratch after that cache. */
    if (observation == 12 || observation == 13 || observation == 14 ||
            observation == 19) {
        image = reuse_image(allocator, observation == 19 ? 4 : 0,
            observation == 19);
        if (image == NULL || reuse_encode(core, image, &priming, &trace,
                0, 0) != SIXEL_OK) goto end;
        sixel_indexed_frame_unref(image);
        image = NULL;
    }
    image = reuse_image(allocator, observation, 0);
    if (image == NULL) goto end;
    sinks[0].reject = observation == 13 || observation == 14;
    reuse_fail = observation == 19;
    status = reuse_encode(core, image, &sinks[0], &trace, observation, 0);
    if (sinks[0].reject || observation == 19) {
        if (status == SIXEL_OK) goto end;
        if (core->vtbl->abort(core) != SIXEL_OK) goto end;
    } else if (status != SIXEL_OK) goto end;
    reuse_fail = 0;
    sixel_indexed_frame_unref(image);
    image = NULL;
    if (observation == 18) {
        if (core->vtbl->abort(core) != SIXEL_OK) goto end;
        ok = sixel_atomic_fetch_add_u32(&reuse_live, 0) == before;
        goto end;
    }
    if (observation == 17) {
        core->vtbl->unref(core);
        core = NULL;
        sixel_allocator_unref(allocator);
        allocator = NULL;
        ok = sixel_atomic_fetch_add_u32(&reuse_live, 0) == 0;
        goto end;
    }
    previous_size = sinks[0].used;
    image = reuse_image(allocator, observation, 1);
    if (image == NULL) goto end;
    trace.arrived = 0;
    before = sixel_atomic_fetch_add_u32(&reuse_allocations, 0);
    status = reuse_encode(core, image, &sinks[1], &trace, observation, 1);
    after = sixel_atomic_fetch_add_u32(&reuse_allocations, 0);
    if (status != SIXEL_OK) goto end;
    if (observation == 15) {
        ok = trace.warm == 1 && trace.cold == 1;
        goto end;
    }
    if (observation == 16) {
        ok = trace.cold == 2 && trace.warm == 0;
        goto end;
    }
    if (observation == 20) {
        ok = trace.warm == 1 && after == before;
        goto end;
    }
    if (observation == 3) {
        ok = sinks[0].used == previous_size && sinks[1].used != 0;
        goto end;
    }
    object = NULL;
    if (sixel_encoder_core_factory_new(allocator, &object) != SIXEL_OK)
        goto end;
    fresh = object;
    if (reuse_encode(fresh, image, &sinks[2], &reference, observation, 1)
            != SIXEL_OK) goto end;
    ok = sinks[1].used == sinks[2].used && sinks[1].used != 0 &&
        memcmp(sinks[1].bytes, sinks[2].bytes, sinks[1].used) == 0;
end:
    if (fresh != NULL) fresh->vtbl->unref(fresh);
    if (core != NULL) core->vtbl->unref(core);
    sixel_indexed_frame_unref(image);
    sixel_allocator_unref(allocator);
    reuse_fail = 0;
    if (ready == 2) sixel_cond_destroy(&trace.cond);
    if (ready != 0) sixel_mutex_destroy(&trace.mutex);
    return ok ? 0 : 1;
#else
    (void)observation;
    return 77;
#endif
}
