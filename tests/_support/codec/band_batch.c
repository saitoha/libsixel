/* SPDX-License-Identifier: MIT */
/* Synthetic clocks drive admission deterministically, without wall-time
 * assertions. A fresh single-band parallel core supplies reference bytes. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <string.h>
#include <6cells.h>
#include "src/encoder-core.h"
#include "src/encode-result.h"
#include "src/palette.h"
#include "src/threading.h"
#include "src/sixel_atomic.h"

#if SIXEL_ENABLE_THREADS
typedef struct batch_fixture {
    sixel_timeline_logger_vtbl_t const *vtbl;
    unsigned char bytes[32768];
    size_t size;
    sixel_atomic_u32_t ticks;
    int maximum;
    int last_band;
    int reject;
    int missing_clock;
} batch_fixture_t;

static double
batch_clock(void *context)
{
    batch_fixture_t *f;

    f = context;
    if (f->missing_clock) return 0.0;
    return 1.0 + (double)sixel_atomic_fetch_add_u32(&f->ticks, 1) * 1e-9;
}

static SIXELSTATUS
batch_log(sixel_timeline_logger_t *self, sixel_timeline_event_t const *event)
{
    batch_fixture_t *f;

    f = (batch_fixture_t *)self;
    if (strcmp(event->event, "job_bands") == 0 &&
            event->job_id > f->maximum) f->maximum = event->job_id;
    if (strcmp(event->event, "dispatch") == 0) f->last_band = event->job_id;
    return SIXEL_OK;
}

static sixel_timeline_logger_vtbl_t const batch_log_vtbl = {
    NULL, NULL, batch_log, NULL, NULL, NULL
};

static SIXELSTATUS
batch_write(char const *bytes, size_t size, void *context)
{
    batch_fixture_t *f;

    f = context;
    if (f->reject || size > sizeof(f->bytes) - f->size)
        return SIXEL_RUNTIME_ERROR;
    memcpy(f->bytes + f->size, bytes, size);
    f->size += size;
    return SIXEL_OK;
}

static sixel_indexed_frame_t *
batch_image(sixel_allocator_t *allocator, int observation)
{
    sixel_wire_color_definition_t colors[2] = {
        {0, 2, {0, 10, 20}}, {1, 2, {80, 90, 100}}
    };
    sixel_wire_palette_request_t request;
    sixel_indexed_frame_description_t desc;
    sixel_wire_palette_t *wire;
    sixel_indexed_frame_t *image;
    size_t i;

    wire = NULL;
    image = NULL;
    memset(&request, 0, sizeof(request));
    request.definitions = colors;
    request.count = 2;
    request.color_definition = SIXEL_PALETTE_OUTPUT_LEGACY;
    request.colorspace = SIXEL_COLORSPACE_GAMMA;
    if (sixel_wire_palette_new(&wire, &request, allocator) != SIXEL_OK)
        return NULL;
    memset(&desc, 0, sizeof(desc));
    desc.width = 16;
    desc.height = observation == 10 ? 119 : 120;
    desc.encoded_x = observation == 3 ? 3 : 0;
    desc.encoded_y = observation == 3 ? 2 : 0;
    desc.encoded_width = desc.width + desc.encoded_x;
    desc.encoded_height = desc.height + desc.encoded_y;
    desc.plane_width = desc.width;
    desc.plane_height = desc.height;
    desc.palette = wire;
    desc.keycolor = -1;
    desc.transparent_background = observation == 3 || observation == 4;
    if (sixel_indexed_frame_new(&image, &desc, allocator) == SIXEL_OK) {
        for (i = 0; i < image->pixels; ++i) {
            image->indexes[i] = (sixel_index_t)((i * 7 + i / 13) % 2);
            image->skip[i] = observation == 4 && i % 3 == 0
                ? SIXEL_INDEXED_KEEP_APPROXIMATE : SIXEL_INDEXED_PAINT;
        }
    }
    sixel_wire_palette_unref(wire);
    return image;
}

static SIXELSTATUS
batch_encode(sixel_encoder_core_t *core, sixel_indexed_frame_t *image,
    batch_fixture_t *f, int threads, int queue, int partial, int aborting)
{
    sixel_encoder_core_prepare_request_t request;
    SIXELSTATUS status;
    int height;

    memset(&request, 0, sizeof(request));
    height = image->description.height;
    request.image = image;
    request.threads = request.worker_capacity = threads;
    request.queue_bands = queue;
    request.options.encode_policy = SIXEL_ENCODEPOLICY_FAST;
    request.sink.write = batch_write;
    request.sink.context = f;
    request.logger = (sixel_timeline_logger_t *)f;
    f->size = 0;
    f->maximum = 0;
    f->last_band = -1;
    status = core->vtbl->prepare(core, &request);
    if (status != SIXEL_OK) return status;
    if (partial || aborting) {
        status = sixel_indexed_frame_publish(image, 19);
        if (status != SIXEL_OK) return status;
        status = core->vtbl->consume(core, 19);
        if (status != SIXEL_OK) return status;
        /* The fourth band is not complete. Dispatch itself is synchronous;
         * this observation needs no wait for native worker scheduling. */
        if (f->last_band != 2) return SIXEL_LOGIC_ERROR;
        if (aborting) return core->vtbl->abort(core);
    }
    status = sixel_indexed_frame_publish(image, height);
    if (status != SIXEL_OK) return status;
    status = sixel_indexed_frame_complete(image);
    if (status != SIXEL_OK) return status;
    status = core->vtbl->consume(core, height);
    if (status != SIXEL_OK) return status;
    return core->vtbl->finish(core);
}
#endif

int
band_batch_observe(int observation)
{
#if SIXEL_ENABLE_THREADS
    sixel_allocator_t *allocator;
    sixel_encoder_core_t *core;
    sixel_encoder_core_t *fresh;
    sixel_indexed_frame_t *image;
    batch_fixture_t f;
    batch_fixture_t reference;
    SIXELSTATUS status;
    void *object;
    int i;
    int warm_frames;
    int queue;
    int expected;
    int ok;

    allocator = NULL;
    core = fresh = NULL;
    image = NULL;
    ok = 0;
    memset(&f, 0, sizeof(f));
    memset(&reference, 0, sizeof(reference));
    f.vtbl = reference.vtbl = &batch_log_vtbl;
    queue = observation == 5 ? 1 : observation == 6 ? 2 : 8;
    expected = observation == 0 || observation == 6 ? 2 : 4;
    if (observation == 5) expected = 1;
    warm_frames = observation == 0 ? 2 : 4;
    if (sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL) != SIXEL_OK)
        goto end;
    object = NULL;
    if (sixel_encoder_core_factory_new(allocator, &object) != SIXEL_OK)
        goto end;
    core = object;
    if (sixel_encoder_core_test_clock(core, batch_clock, &f) != SIXEL_OK)
        goto end;
    for (i = 0; i < warm_frames; ++i) {
        image = batch_image(allocator, observation);
        if (image == NULL) goto end;
        if (batch_encode(core, image, &f, 2, queue, 0, 0) != SIXEL_OK)
            goto end;
        sixel_indexed_frame_unref(image);
        image = NULL;
    }
    image = batch_image(allocator, observation);
    if (image == NULL) goto end;
    f.reject = observation == 8;
    f.missing_clock = observation == 9;
    status = batch_encode(core, image, &f, 2, queue,
        observation == 2, observation == 7);
    if (observation == 8) {
        if (status == SIXEL_OK || f.maximum != 4) goto end;
        if (core->vtbl->abort(core) != SIXEL_OK) goto end;
    } else if (status != SIXEL_OK) goto end;
    if (observation == 7 || observation == 8 || observation == 9) {
        if (observation == 9 && f.maximum != 4) goto end;
        sixel_indexed_frame_unref(image);
        image = batch_image(allocator, observation);
        if (image == NULL) goto end;
        f.reject = f.missing_clock = 0;
        if (batch_encode(core, image, &f, 2, queue, 0, 0) != SIXEL_OK)
            goto end;
        expected = 1;
    }
    if (f.maximum != expected) goto end;
    if (observation == 2) {
        ok = 1;
        goto end;
    }
    sixel_indexed_frame_unref(image);
    image = batch_image(allocator, observation);
    if (image == NULL) goto end;
    object = NULL;
    if (sixel_encoder_core_factory_new(allocator, &object) != SIXEL_OK)
        goto end;
    fresh = object;
    if (batch_encode(fresh, image, &reference, 2, queue, 0, 0) != SIXEL_OK)
        goto end;
    ok = f.size != 0 && f.size == reference.size &&
        memcmp(f.bytes, reference.bytes, f.size) == 0;
end:
    if (fresh != NULL) fresh->vtbl->unref(fresh);
    if (core != NULL) core->vtbl->unref(core);
    sixel_indexed_frame_unref(image);
    sixel_allocator_unref(allocator);
    return ok ? 0 : 1;
#else
    (void)observation;
    return 77;
#endif
}
