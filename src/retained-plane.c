/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdint.h>
#include <string.h>
#include "encode-result.h"
#include "threading.h"

typedef struct sixel_retained_plane_storage {
    sixel_retained_plane_interface_t interface;
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
    sixel_mutex_destroy(&plane->gate);
    sixel_allocator_free(allocator, plane->rgb);
    sixel_allocator_free(allocator, plane->valid);
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
    sixel_allocator_free(plane->allocator, plane->rgb);
    sixel_allocator_free(plane->allocator, plane->valid);
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
    sixel_retained_plane_unref(&plane->interface);
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
            request->image == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    plane = (sixel_retained_plane_storage_t *)self;
    image = request->image;
    description = &image->description;
    sixel_wire_palette_get_view(description->palette, &palette);
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
    (void)sixel_retained_plane_abort(&plane->interface, update);
    sixel_indexed_frame_unref(update->image);
    sixel_allocator_free(plane->allocator, update->paint);
    sixel_allocator_free(plane->allocator, update);
    sixel_retained_plane_unref(&plane->interface);
}

static SIXELSTATUS
sixel_retained_plane_commit(sixel_retained_plane_interface_t *self,
    sixel_retained_update_t *update, sixel_stream_receipt_t const *receipt,
    sixel_retained_update_stats_t *stats)
{
    sixel_retained_plane_storage_t *plane;
    sixel_indexed_frame_t *image;
    sixel_indexed_frame_description_t const *description;
    sixel_wire_palette_view_t palette;
    sixel_retained_update_stats_t result;
    size_t index;
    size_t target;
    int x;
    int y;
    unsigned int skip;
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
    description = &image->description;
    sixel_wire_palette_get_view(description->palette, &palette);
    memset(&result, 0, sizeof(result));
    result.paint = update->paint;
    result.paint_size = update->paint != NULL ? image->pixels : 0;
    /* Validation and storage reservation precede this loop. Nothing below
     * can fail or allocate: expand only painted indexes into their final
     * destination, preserving holes and unrelated pixels outside the frame. */
    index = 0;
    for (y = 0; y < description->height; ++y) {
        target = (size_t)(description->origin_y + y) *
            (size_t)plane->description.width + (size_t)description->origin_x;
        for (x = 0; x < description->width; ++x, ++index, ++target) {
            skip = image->skip != NULL ? image->skip[index] : 0;
            if (skip != 0) {
                result.refinement_needed |=
                    skip == SIXEL_INDEXED_KEEP_APPROXIMATE;
                continue;
            }
            memcpy(plane->rgb + target * 3,
                palette.colors[image->indexes[index]].emitted_rgb, 3);
            plane->valid[target] = 1;
            if (update->paint != NULL) {
                update->paint[index] = 1;
            }
            ++result.painted_pixels;
        }
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
    sixel_retained_plane_acquire_read,
    sixel_retained_plane_begin_update,
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
    plane->interface.vtbl = &g_retained_plane_vtbl;
    plane->ref = 1;
    plane->allocator = allocator;
    plane->stream = stream;
    sixel_allocator_ref(allocator);
    *object = &plane->interface;
    return SIXEL_OK;
}

/* Legacy highcolor/repair adapters borrow an exclusive mutable view. They
 * already hold the encoder operation gate; the plane gate also excludes
 * typed readers and indexed commits. No second retained allocation exists. */
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
