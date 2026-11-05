/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdint.h>
#include <string.h>
#include "encode-result.h"
#include "threading.h"
#include "retained-plane-rows.h"
#include "retained-plane-workers.h"
#include "retained-plane-gpu.h"

typedef struct sixel_retained_plane_storage {
    sixel_retained_plane_interface_t iface;
    sixel_atomic_u32_t ref;
    sixel_allocator_t *allocator;
    sixel_mutex_t gate;
    sixel_retained_plane_description_t description;
    unsigned char *rgb;
    unsigned char *valid;
    size_t pixels;
    uint64_t stream;
    uint64_t epoch;
    uint64_t sequence;
    unsigned int readers;
    sixel_retained_update_t *update;
    sixel_retained_workers_t *workers;
    sixel_retained_gpu_t *gpu;
    int gpu_test_fault;
} sixel_retained_plane_storage_t;

struct sixel_retained_read {
    sixel_atomic_u32_t ref;
    sixel_retained_plane_storage_t *plane;
    sixel_retained_read_view_t view;
};

struct sixel_retained_update {
    sixel_atomic_u32_t ref;
    sixel_retained_plane_storage_t *plane;
    sixel_indexed_frame_t *image;
    sixel_encode_identity_t identity;
    unsigned char *paint;
    sixel_retained_rows_t rows;
    int worker_threads;
    int staging;
    int staged;
    sixel_retained_update_stats_t gpu_stats;
};

/* A ticket gate is used only when a plane is constructed. The 64-bit stream
 * counter never wraps or reuses an identity, including after plane teardown.
 * It needs no platform-specific global mutex initializer. */
static sixel_atomic_u32_t g_retained_ticket;
static sixel_atomic_u32_t g_retained_serving;
static uint64_t g_retained_stream;

static uint64_t
sixel_retained_next_stream(void)
{
    unsigned int ticket;
    uint64_t stream;

    ticket = sixel_atomic_fetch_add_u32(&g_retained_ticket, 1);
    while (sixel_atomic_fetch_add_u32(&g_retained_serving, 0) != ticket) {
        /* Only other constructors hold this bounded, allocation-free gate. */
    }
    stream = g_retained_stream == UINT64_MAX ? 0 : ++g_retained_stream;
    (void)sixel_atomic_fetch_add_u32(&g_retained_serving, 1);
    return stream;
}

static int
sixel_retained_identity_equal(sixel_encode_identity_t const *a,
    sixel_encode_identity_t const *b)
{
    return a->stream == b->stream && a->epoch == b->epoch &&
        a->base_sequence == b->base_sequence && a->sequence == b->sequence;
}

static void
sixel_retained_plane_ref(sixel_retained_plane_interface_t *self)
{
    sixel_retained_plane_storage_t *plane;

    plane = (sixel_retained_plane_storage_t *)self;
    if (plane != NULL) {
        (void)sixel_atomic_fetch_add_u32(&plane->ref, 1);
    }
}

/* CPU views either own allocator memory or borrow the active GPU buffer.
 * Keep this distinction local to RetainedPlane, including legacy access. */
static void
sixel_retained_plane_free_pixels(sixel_retained_plane_storage_t *plane)
{
    if (plane->gpu != NULL) {
        sixel_retained_gpu_free(plane->gpu);
        plane->gpu = NULL;
    } else {
        sixel_allocator_free(plane->allocator, plane->rgb);
        sixel_allocator_free(plane->allocator, plane->valid);
    }
    plane->rgb = plane->valid = NULL;
}

static void
sixel_retained_plane_unref(sixel_retained_plane_interface_t *self)
{
    sixel_retained_plane_storage_t *plane;
    sixel_allocator_t *allocator;

    plane = (sixel_retained_plane_storage_t *)self;
    if (plane == NULL || sixel_atomic_fetch_sub_u32(&plane->ref, 1) != 1) {
        return;
    }
    allocator = plane->allocator;
    sixel_retained_workers_free(plane->workers);
    sixel_mutex_destroy(&plane->gate);
    sixel_retained_plane_free_pixels(plane);
    sixel_allocator_free(allocator, plane);
    sixel_allocator_unref(allocator);
}

static SIXELSTATUS
sixel_retained_plane_configure(sixel_retained_plane_interface_t *self,
    sixel_retained_plane_description_t const *description)
{
    sixel_retained_plane_storage_t *plane;
    unsigned char *rgb;
    unsigned char *valid;
    size_t pixels;
    SIXELSTATUS status;

    if (self == NULL || description == NULL || description->width <= 0 ||
            description->height <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    if ((size_t)description->height > SIZE_MAX /
            (size_t)description->width / 3) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixels = (size_t)description->width * (size_t)description->height;
    plane = (sixel_retained_plane_storage_t *)self;
    sixel_mutex_lock(&plane->gate);
    status = SIXEL_LOGIC_ERROR;
    if (plane->readers != 0 || plane->update != NULL) {
        goto end;
    }
    if (plane->rgb != NULL &&
            plane->description.width == description->width &&
            plane->description.height == description->height &&
            plane->description.colorspace == description->colorspace &&
            plane->description.color_definition ==
                description->color_definition) {
        status = SIXEL_OK;
        goto end;
    }
    if (plane->epoch == UINT64_MAX) {
        goto end;
    }
    /* Reserve both allocations before altering the last committed plane. */
    rgb = sixel_allocator_calloc(plane->allocator, pixels, 3);
    valid = sixel_allocator_calloc(plane->allocator, pixels, 1);
    if (rgb == NULL || valid == NULL) {
        sixel_allocator_free(plane->allocator, rgb);
        sixel_allocator_free(plane->allocator, valid);
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    sixel_retained_plane_free_pixels(plane);
    plane->rgb = rgb;
    plane->valid = valid;
    plane->pixels = pixels;
    plane->description = *description;
    ++plane->epoch;
    plane->sequence = 0;
    status = SIXEL_OK;
end:
    sixel_mutex_unlock(&plane->gate);
    return status;
}

static SIXELSTATUS
sixel_retained_plane_set_device(sixel_retained_plane_interface_t *self,
    sixel_gpu_device_interface_t *device)
{
    sixel_retained_plane_storage_t *plane;
    sixel_retained_gpu_t *gpu;
    sixel_retained_gpu_read_view_t view;
    unsigned char *rgb;
    unsigned char *valid;
    SIXELSTATUS status;

    if (self == NULL) return SIXEL_BAD_ARGUMENT;
    plane = (sixel_retained_plane_storage_t *)self;
    sixel_mutex_lock(&plane->gate);
    status = SIXEL_LOGIC_ERROR;
    if (plane->readers != 0 || plane->update != NULL || plane->rgb == NULL)
        goto end;
    status = SIXEL_OK;
    if (device == NULL && plane->gpu == NULL) goto end;
    if (device != NULL && sixel_retained_gpu_read(plane->gpu, device,
            &view) == SIXEL_OK) goto end;
    if (device != NULL) {
        gpu = NULL;
        status = sixel_retained_gpu_new(device, &plane->description,
            plane->rgb, plane->valid, &gpu);
        if (status != SIXEL_OK) goto end;
        sixel_retained_plane_free_pixels(plane);
        plane->gpu = gpu;
        sixel_retained_gpu_map(gpu, &plane->rgb, &plane->valid);
        sixel_retained_workers_free(plane->workers);
        plane->workers = NULL;
    } else {
        /* Reserve the destination before retiring coherent GPU storage.
         * Quiescence makes both views observe the same last accepted frame. */
        rgb = sixel_allocator_malloc(plane->allocator, plane->pixels * 3);
        valid = sixel_allocator_malloc(plane->allocator, plane->pixels);
        if (rgb == NULL || valid == NULL) {
            sixel_allocator_free(plane->allocator, rgb);
            sixel_allocator_free(plane->allocator, valid);
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        memcpy(rgb, plane->rgb, plane->pixels * 3);
        memcpy(valid, plane->valid, plane->pixels);
        sixel_retained_plane_free_pixels(plane);
        plane->rgb = rgb;
        plane->valid = valid;
    }
end:
    sixel_mutex_unlock(&plane->gate);
    return status;
}

static SIXELSTATUS
sixel_retained_plane_acquire_read(sixel_retained_plane_interface_t *self,
    sixel_retained_read_t **result)
{
    sixel_retained_plane_storage_t *plane;
    sixel_retained_read_t *read;

    if (self == NULL || result == NULL || *result != NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    plane = (sixel_retained_plane_storage_t *)self;
    sixel_mutex_lock(&plane->gate);
    if (plane->rgb == NULL) {
        sixel_mutex_unlock(&plane->gate);
        return SIXEL_LOGIC_ERROR;
    }
    read = sixel_allocator_calloc(plane->allocator, 1, sizeof(*read));
    if (read == NULL) {
        sixel_mutex_unlock(&plane->gate);
        return SIXEL_BAD_ALLOCATION;
    }
    read->ref = 1;
    read->plane = plane;
    read->view.rgb = plane->rgb;
    read->view.valid = plane->valid;
    read->view.rgb_stride = (size_t)plane->description.width * 3;
    read->view.valid_stride = (size_t)plane->description.width;
    read->view.description = plane->description;
    read->view.stream = plane->stream;
    read->view.epoch = plane->epoch;
    read->view.sequence = plane->sequence;
    ++plane->readers;
    sixel_retained_plane_ref(self);
    sixel_mutex_unlock(&plane->gate);
    *result = read;
    return SIXEL_OK;
}

SIXELAPI void
sixel_retained_read_ref(sixel_retained_read_t *read)
{
    if (read != NULL) {
        (void)sixel_atomic_fetch_add_u32(&read->ref, 1);
    }
}

SIXELAPI void
sixel_retained_read_unref(sixel_retained_read_t *read)
{
    sixel_retained_plane_storage_t *plane;

    if (read == NULL || sixel_atomic_fetch_sub_u32(&read->ref, 1) != 1) {
        return;
    }
    plane = read->plane;
    sixel_mutex_lock(&plane->gate);
    --plane->readers;
    sixel_mutex_unlock(&plane->gate);
    sixel_allocator_free(plane->allocator, read);
    sixel_retained_plane_unref(&plane->iface);
}

SIXELAPI SIXELSTATUS
sixel_retained_read_get_view(sixel_retained_read_t const *read,
    sixel_retained_read_view_t *view)
{
    if (read == NULL || view == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *view = read->view;
    return SIXEL_OK;
}

SIXELAPI SIXELSTATUS
sixel_retained_read_get_gpu_view(sixel_retained_read_t const *read,
    sixel_gpu_device_interface_t *device, sixel_retained_gpu_read_view_t *view)
{
    if (read == NULL || device == NULL || view == NULL)
        return SIXEL_BAD_ARGUMENT;
    /* A live read excludes reconfiguration, commit and invalidation. */
    return sixel_retained_gpu_read(read->plane->gpu, device, view);
}

/* Bound work by both the caller's current budget and rectangle geometry.
 * The row grain amortizes wakeup/reduction cost without making a fixed worker
 * count the policy. A changed budget never creates workers in commit. */
static int
sixel_retained_plane_prepare_workers(sixel_retained_plane_storage_t *plane,
    sixel_indexed_frame_t const *image, int limit)
{
#if SIXEL_ENABLE_THREADS
    sixel_retained_workers_t *replacement;
    size_t chunks;
    int capacity;
    int hardware;

    if (limit < 2) return 1;
    chunks = 1 + (image->pixels - 1) / 524288;
    if (chunks < (size_t)limit) limit = (int)chunks;
    if (limit > image->description.height) limit = image->description.height;
    hardware = sixel_get_hw_threads();
    if (hardware > 0 && limit > hardware) limit = hardware;
    if (limit < 2) return 1;
    capacity = sixel_retained_workers_capacity(plane->workers);
    if (capacity < limit) {
        replacement = sixel_retained_workers_new(plane->allocator, limit);
        if (replacement == NULL) return 1;
        sixel_retained_workers_free(plane->workers);
        plane->workers = replacement;
        capacity = limit;
    }
    /* Parallelism is optional. Failed admission leaves the existing executor
     * and committed image intact, and cannot turn commit into a retry. */
    return capacity < limit ? capacity : limit;
#else
    (void)plane;
    (void)image;
    (void)limit;
    return 1;
#endif
}

static SIXELSTATUS
sixel_retained_plane_begin_update(sixel_retained_plane_interface_t *self,
    sixel_retained_update_request_t const *request,
    sixel_retained_update_t **result)
{
    sixel_retained_plane_storage_t *plane;
    sixel_retained_update_t *update;
    sixel_indexed_frame_t *image;
    sixel_indexed_frame_description_t const *description;
    sixel_wire_palette_view_t palette;
    sixel_encode_identity_t identity;
    SIXELSTATUS status;
    int state;

    if (self == NULL || request == NULL || result == NULL ||
            *result != NULL || request->baseline == NULL ||
            request->image == NULL || request->worker_threads < 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    plane = (sixel_retained_plane_storage_t *)self;
    image = request->image;
    description = &image->description;
    status = sixel_wire_palette_get_view(description->palette, &palette);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    sixel_mutex_lock(&plane->gate);
    status = SIXEL_LOGIC_ERROR;
    state = sixel_indexed_frame_get_state(image);
    if (plane->update != NULL || image->update != NULL ||
            request->baseline->plane != plane ||
            request->baseline->view.epoch != plane->epoch ||
            request->baseline->view.sequence != plane->sequence ||
            request->sequence <= plane->sequence ||
            state == SIXEL_INDEXED_FAILED ||
            sixel_atomic_fetch_add_u32(&image->users, 0) != 0) {
        goto end;
    }
    status = SIXEL_BAD_ARGUMENT;
    if (description->plane_width != plane->description.width ||
            description->plane_height != plane->description.height ||
            palette.color_definition != plane->description.color_definition ||
            palette.colorspace != plane->description.colorspace) {
        goto end;
    }
    identity.stream = plane->stream;
    identity.epoch = plane->epoch;
    identity.base_sequence = plane->sequence;
    identity.sequence = request->sequence;
    if ((state == SIXEL_INDEXED_COMPLETE || image->identity.stream != 0 ||
            sixel_atomic_load_relaxed_i32(&image->rows) != 0) &&
            !sixel_retained_identity_equal(&identity, &image->identity)) {
        status = SIXEL_LOGIC_ERROR;
        goto end;
    }
    update = sixel_allocator_calloc(plane->allocator, 1, sizeof(*update));
    if (update == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    if (request->record_paint) {
        update->paint = sixel_allocator_calloc(plane->allocator,
            image->pixels, 1);
        if (update->paint == NULL) {
            sixel_allocator_free(plane->allocator, update);
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
    }
    update->rows.width = description->width;
    update->rows.stride = (size_t)plane->description.width;
    update->rows.indexes = image->indexes;
    update->rows.skip = image->skip;
    update->rows.paint = update->paint;
    update->rows.rgb = plane->rgb + 3 *
        ((size_t)description->origin_y * update->rows.stride +
         (size_t)description->origin_x);
    update->rows.valid = plane->valid +
        (size_t)description->origin_y * update->rows.stride +
        (size_t)description->origin_x;
    sixel_retained_rows_prepare(&update->rows, &palette);
    if (plane->gpu != NULL) {
        status = sixel_retained_gpu_prepare(plane->gpu, description,
            &palette, request->record_paint);
        if (status != SIXEL_OK) {
            sixel_allocator_free(plane->allocator, update->paint);
            sixel_allocator_free(plane->allocator, update);
            goto end;
        }
    } else {
        update->worker_threads = sixel_retained_plane_prepare_workers(plane,
            image, request->worker_threads);
    }
    update->ref = 1;
    update->plane = plane;
    update->image = image;
    update->identity = identity;
    image->identity = identity;
    sixel_indexed_frame_ref(image);
    sixel_retained_plane_ref(self);
    plane->update = update;
    image->update = update;
    *result = update;
    status = SIXEL_OK;
end:
    sixel_mutex_unlock(&plane->gate);
    return status;
}

static SIXELSTATUS
sixel_retained_plane_abort(sixel_retained_plane_interface_t *self,
    sixel_retained_update_t *update)
{
    sixel_retained_plane_storage_t *plane;
    SIXELSTATUS status;

    if (self == NULL || update == NULL ||
            update->plane != (sixel_retained_plane_storage_t *)self) {
        return SIXEL_BAD_ARGUMENT;
    }
    plane = update->plane;
    sixel_mutex_lock(&plane->gate);
    status = SIXEL_LOGIC_ERROR;
    if (plane->update == update &&
            sixel_atomic_fetch_add_u32(&update->image->users, 0) == 0) {
        plane->update = NULL;
        update->image->update = NULL;
        status = SIXEL_OK;
    }
    sixel_mutex_unlock(&plane->gate);
    return status;
}

SIXELAPI void
sixel_retained_update_ref(sixel_retained_update_t *update)
{
    if (update != NULL) {
        (void)sixel_atomic_fetch_add_u32(&update->ref, 1);
    }
}

SIXELAPI void
sixel_retained_update_unref(sixel_retained_update_t *update)
{
    sixel_retained_plane_storage_t *plane;

    if (update == NULL || sixel_atomic_fetch_sub_u32(&update->ref, 1) != 1) {
        return;
    }
    plane = update->plane;
    /* Resource-user references defer final abort until workers drain. */
    (void)sixel_retained_plane_abort(&plane->iface, update);
    sixel_indexed_frame_unref(update->image);
    sixel_allocator_free(plane->allocator, update->paint);
    sixel_allocator_free(plane->allocator, update);
    sixel_retained_plane_unref(&plane->iface);
}

/* Fault injection observes candidate isolation before or after GPU completion.
 * The caller owns this plane; no process-global failure state is introduced. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_retained_plane_test_gpu_fault(sixel_retained_plane_interface_t *self,
    int fault)
{
    sixel_retained_plane_storage_t *plane;

    if (self == NULL || fault < 0 || fault > 2) return SIXEL_BAD_ARGUMENT;
    plane = (sixel_retained_plane_storage_t *)self;
    sixel_mutex_lock(&plane->gate);
    if (plane->update != NULL && plane->update->staging) {
        sixel_mutex_unlock(&plane->gate);
        return SIXEL_LOGIC_ERROR;
    }
    plane->gpu_test_fault = fault;
    sixel_mutex_unlock(&plane->gate);
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_retained_plane_stage(sixel_retained_plane_interface_t *self,
    sixel_retained_update_t *update)
{
    sixel_retained_plane_storage_t *plane;
    SIXELSTATUS status;

    if (self == NULL || update == NULL ||
            update->plane != (sixel_retained_plane_storage_t *)self)
        return SIXEL_BAD_ARGUMENT;
    plane = update->plane;
    sixel_mutex_lock(&plane->gate);
    if (plane->update != update || update->staging ||
            sixel_indexed_frame_get_state(update->image) !=
                SIXEL_INDEXED_COMPLETE) {
        sixel_mutex_unlock(&plane->gate);
        return SIXEL_LOGIC_ERROR;
    }
    if (plane->gpu == NULL || update->staged) {
        sixel_mutex_unlock(&plane->gate);
        return SIXEL_OK;
    }
    update->staging = 1;
    sixel_indexed_frame_acquire_user(update->image);
    sixel_mutex_unlock(&plane->gate);
    /* RetainedPlane owns the kernel and candidate. The resource-user lease
     * prevents abort/reconfiguration while GPU completion waits outside gate;
     * independent body workers can still read immutable indexes and skips. */
    status = sixel_retained_gpu_stage(plane->gpu, update->image->indexes,
        update->image->skip, update->paint, &update->gpu_stats,
        plane->gpu_test_fault);
    sixel_mutex_lock(&plane->gate);
    update->staged = status == SIXEL_OK;
    update->staging = 0;
    sixel_mutex_unlock(&plane->gate);
    sixel_indexed_frame_release_user(update->image);
    return status;
}

static SIXELSTATUS
sixel_retained_plane_commit(sixel_retained_plane_interface_t *self,
    sixel_retained_update_t *update, sixel_stream_receipt_t const *receipt,
    sixel_retained_update_stats_t *stats)
{
    sixel_retained_plane_storage_t *plane;
    sixel_indexed_frame_t *image;
    sixel_retained_update_stats_t result;
    SIXELSTATUS status;

    if (self == NULL || update == NULL || receipt == NULL || stats == NULL ||
            update->plane != (sixel_retained_plane_storage_t *)self) {
        return SIXEL_BAD_ARGUMENT;
    }
    plane = update->plane;
    image = update->image;
    sixel_mutex_lock(&plane->gate);
    status = SIXEL_LOGIC_ERROR;
    if (plane->update != update || plane->readers != 0 ||
            sixel_atomic_fetch_add_u32(&image->users, 0) != 0 ||
            sixel_indexed_frame_get_state(image) != SIXEL_INDEXED_COMPLETE ||
            plane->stream != update->identity.stream ||
            plane->epoch != update->identity.epoch ||
            plane->sequence != update->identity.base_sequence ||
            !sixel_retained_identity_equal(&update->identity,
                &receipt->identity) ||
            !sixel_retained_identity_equal(&update->identity,
                &image->identity)) {
        goto end;
    }
    if (plane->gpu != NULL && !update->staged) goto end;
    memset(&result, 0, sizeof(result));
    if (plane->gpu != NULL) result = update->gpu_stats;
    result.paint = update->paint;
    result.paint_size = update->paint != NULL ? image->pixels : 0;
    /* Receipt validation precedes publication. GPU candidate construction
     * already completed; CPU expansion remains allocation-free. Neither path
     * can fail after changing the committed image or its stream identity. */
    if (plane->gpu != NULL) {
        sixel_retained_gpu_commit(plane->gpu);
        sixel_retained_gpu_map(plane->gpu, &plane->rgb, &plane->valid);
    } else {
        sixel_retained_workers_run(plane->workers, &update->rows,
            image->description.height, update->worker_threads, &result);
    }
    plane->sequence = update->identity.sequence;
    plane->update = NULL;
    image->update = NULL;
    *stats = result;
    status = SIXEL_OK;
end:
    sixel_mutex_unlock(&plane->gate);
    return status;
}

static SIXELSTATUS
sixel_retained_plane_invalidate(sixel_retained_plane_interface_t *self,
    sixel_retained_invalidation_t reason)
{
    sixel_retained_plane_storage_t *plane;
    SIXELSTATUS status;

    if (self == NULL || reason < SIXEL_RETAINED_SEEK ||
            reason > SIXEL_RETAINED_QUEUE_DISCARD) {
        return SIXEL_BAD_ARGUMENT;
    }
    plane = (sixel_retained_plane_storage_t *)self;
    sixel_mutex_lock(&plane->gate);
    status = SIXEL_LOGIC_ERROR;
    if (plane->readers == 0 && plane->update == NULL &&
            plane->epoch != UINT64_MAX) {
        if (plane->valid != NULL) {
            memset(plane->valid, 0, plane->pixels);
        }
        sixel_retained_gpu_cpu_write(plane->gpu);
        ++plane->epoch;
        plane->sequence = 0;
        status = SIXEL_OK;
    }
    sixel_mutex_unlock(&plane->gate);
    return status;
}

static sixel_retained_plane_vtbl_t const g_retained_plane_vtbl = {
    sixel_retained_plane_ref,
    sixel_retained_plane_unref,
    sixel_retained_plane_configure,
    sixel_retained_plane_set_device,
    sixel_retained_plane_acquire_read,
    sixel_retained_plane_begin_update,
    sixel_retained_plane_stage,
    sixel_retained_plane_commit,
    sixel_retained_plane_abort,
    sixel_retained_plane_invalidate
};

SIXEL_INTERNAL_API SIXELSTATUS
sixel_retained_plane_factory_new(sixel_allocator_t *allocator, void **object)
{
    sixel_retained_plane_storage_t *plane;
    uint64_t stream;

    if (allocator == NULL || object == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *object = NULL;
    stream = sixel_retained_next_stream();
    if (stream == 0) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    plane = sixel_allocator_calloc(allocator, 1, sizeof(*plane));
    if (plane == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
#if SIXEL_ENABLE_THREADS
    if (sixel_mutex_init(&plane->gate) != 0) {
        sixel_allocator_free(allocator, plane);
        return SIXEL_RUNTIME_ERROR;
    }
#endif
    plane->iface.vtbl = &g_retained_plane_vtbl;
    plane->ref = 1;
    plane->allocator = allocator;
    plane->stream = stream;
    sixel_allocator_ref(allocator);
    *object = &plane->iface;
    return SIXEL_OK;
}

/* Device migration and candidate publication can replace the committed
 * allocation. Refresh legacy aliases after the typed session has drained;
 * this read-only operation neither advances identity nor dirties a candidate.
 * The encoder operation gate excludes the next transaction during use. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_retained_plane_legacy_rebind(sixel_retained_plane_interface_t *self,
    unsigned char **rgb, unsigned char **valid)
{
    sixel_retained_plane_storage_t *plane;

    if (self == NULL || rgb == NULL || valid == NULL)
        return SIXEL_BAD_ARGUMENT;
    plane = (sixel_retained_plane_storage_t *)self;
    sixel_mutex_lock(&plane->gate);
    if (plane->update != NULL || plane->rgb == NULL) {
        sixel_mutex_unlock(&plane->gate);
        return SIXEL_LOGIC_ERROR;
    }
    *rgb = plane->rgb;
    *valid = plane->valid;
    sixel_mutex_unlock(&plane->gate);
    return SIXEL_OK;
}

/* Legacy highcolor/repair adapters borrow an exclusive mutable view. They
 * already hold the encoder operation gate; the plane gate also excludes
 * typed readers and indexed commits. They borrow committed storage only;
 * the private GPU candidate never becomes a legacy write destination. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_retained_plane_legacy_map(sixel_retained_plane_interface_t *self,
    unsigned char **rgb, unsigned char **valid)
{
    sixel_retained_plane_storage_t *plane;

    if (self == NULL || rgb == NULL || valid == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    plane = (sixel_retained_plane_storage_t *)self;
    sixel_mutex_lock(&plane->gate);
    if (plane->readers != 0 || plane->update != NULL || plane->rgb == NULL ||
            plane->sequence == UINT64_MAX) {
        sixel_mutex_unlock(&plane->gate);
        return SIXEL_LOGIC_ERROR;
    }
    sixel_retained_gpu_cpu_write(plane->gpu);
    *rgb = plane->rgb;
    *valid = plane->valid;
    return SIXEL_OK;
}

SIXEL_INTERNAL_API void
sixel_retained_plane_legacy_unmap(sixel_retained_plane_interface_t *self)
{
    sixel_retained_plane_storage_t *plane;

    plane = (sixel_retained_plane_storage_t *)self;
    ++plane->sequence;
    sixel_mutex_unlock(&plane->gate);
}
