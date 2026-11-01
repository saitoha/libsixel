/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <limits.h>
#include <string.h>
#include "encode-result.h"
#include "colorspace.h"
#include "palette.h"

struct sixel_wire_palette {
    sixel_atomic_u32_t ref;
    sixel_allocator_t *allocator;
    sixel_wire_palette_view_t view;
    sixel_wire_color_t colors[1];
};

SIXELAPI SIXELSTATUS
sixel_wire_palette_new(sixel_wire_palette_t **result,
    sixel_wire_palette_request_t const *request,
    sixel_allocator_t *allocator)
{
    sixel_wire_palette_t *wire;
    sixel_wire_color_definition_t const *definition;
    unsigned char codes[3];
    size_t i;
    size_t j;
    size_t size;
    int channel;

    if (result == NULL || *result != NULL || request == NULL ||
            allocator == NULL || request->definitions == NULL ||
            request->count == 0 || request->count > SIXEL_PALETTE_MAX) {
        return SIXEL_BAD_ARGUMENT;
    }
    /* These fields describe the receiver's interpretation, not the lookup
     * workspace. Reject interpretations this byte history cannot model. */
    if (request->color_definition != SIXEL_PALETTE_OUTPUT_LEGACY ||
            (request->colorspace != SIXEL_COLORSPACE_GAMMA &&
             request->colorspace != SIXEL_COLORSPACE_LINEAR &&
             request->colorspace != SIXEL_COLORSPACE_SMPTEC)) {
        return SIXEL_BAD_ARGUMENT;
    }
    for (i = 0; i < request->count; ++i) {
        definition = &request->definitions[i];
        if (definition->register_index >= SIXEL_PALETTE_MAX ||
                (definition->type != 1 && definition->type != 2) ||
                definition->fields[0] < 0 ||
                definition->fields[0] > (definition->type == 1 ? 360 : 100) ||
                definition->fields[1] < 0 || definition->fields[1] > 100 ||
                definition->fields[2] < 0 || definition->fields[2] > 100) {
            return SIXEL_BAD_ARGUMENT;
        }
        for (j = 0; j < i; ++j) {
            if (request->definitions[j].register_index ==
                    definition->register_index) {
                return SIXEL_BAD_ARGUMENT;
            }
        }
    }
    size = sizeof(*wire) +
        (request->count - 1) * sizeof(wire->colors[0]);
    wire = sixel_allocator_malloc(allocator, size);
    if (wire == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    wire->ref = 1;
    wire->allocator = allocator;
    sixel_allocator_ref(allocator);
    wire->view.colors = wire->colors;
    wire->view.count = request->count;
    wire->view.color_definition = request->color_definition;
    wire->view.colorspace = request->colorspace;
    for (i = 0; i < request->count; ++i) {
        definition = &request->definitions[i];
        wire->colors[i].definition = *definition;
        if (definition->type == 1) {
            sixel_color_hls_to_rgb101(codes, definition->fields);
        } else {
            for (channel = 0; channel < 3; ++channel) {
                codes[channel] = (unsigned char)definition->fields[channel];
            }
        }
        for (channel = 0; channel < 3; ++channel) {
            wire->colors[i].emitted_rgb[channel] =
                (unsigned char)((codes[channel] * 255U + 50U) / 100U);
        }
    }
    *result = wire;
    return SIXEL_OK;
}

SIXELAPI void
sixel_wire_palette_ref(sixel_wire_palette_t *wire)
{
    if (wire != NULL) {
        (void)sixel_atomic_fetch_add_u32(&wire->ref, 1);
    }
}

SIXELAPI void
sixel_wire_palette_unref(sixel_wire_palette_t *wire)
{
    sixel_allocator_t *allocator;

    if (wire != NULL &&
            sixel_atomic_fetch_sub_u32(&wire->ref, 1) == 1) {
        allocator = wire->allocator;
        sixel_allocator_free(allocator, wire);
        sixel_allocator_unref(allocator);
    }
}

SIXELAPI SIXELSTATUS
sixel_wire_palette_get_view(sixel_wire_palette_t const *wire,
    sixel_wire_palette_view_t *view)
{
    if (wire == NULL || view == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *view = wire->view;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_indexed_description_validate(
    sixel_indexed_frame_description_t const *description, size_t *pixels)
{
    if (description == NULL || description->palette == NULL ||
            description->width <= 0 || description->height <= 0 ||
            description->encoded_x < 0 || description->encoded_y < 0 ||
            description->origin_x < 0 || description->origin_y < 0 ||
            description->encoded_width < description->width ||
            description->encoded_height < description->height ||
            description->plane_width < description->width ||
            description->plane_height < description->height ||
            description->encoded_x >
                description->encoded_width - description->width ||
            description->encoded_y >
                description->encoded_height - description->height ||
            description->origin_x >
                description->plane_width - description->width ||
            description->origin_y >
                description->plane_height - description->height ||
            description->keycolor < -1 ||
            description->keycolor >= (int)description->palette->view.count) {
        return SIXEL_BAD_ARGUMENT;
    }
    if ((size_t)description->height > SIZE_MAX /
            (size_t)description->width / sizeof(sixel_index_t)) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    *pixels = (size_t)description->width * (size_t)description->height;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_indexed_frame_allocate(sixel_indexed_frame_t **result,
    sixel_indexed_frame_description_t const *description,
    sixel_allocator_t *allocator, int allocate_pixels)
{
    sixel_indexed_frame_t *image;
    size_t pixels;
    SIXELSTATUS status;

    if (result == NULL || *result != NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    status = sixel_indexed_description_validate(description, &pixels);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    image = sixel_allocator_calloc(allocator, 1, sizeof(*image));
    if (image == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    image->ref = 1;
    image->allocator = allocator;
    image->description = *description;
    image->pixels = pixels;
    sixel_atomic_init_i32(&image->rows, 0);
    sixel_atomic_init_i32(&image->state, SIXEL_INDEXED_BUILDING);
    image->users = 0;
    sixel_allocator_ref(allocator);
    sixel_wire_palette_ref(description->palette);
    if (allocate_pixels) {
        image->indexes = sixel_allocator_malloc(allocator,
            pixels * sizeof(*image->indexes));
        image->skip = sixel_allocator_calloc(allocator, pixels, 1);
        if (image->indexes == NULL || image->skip == NULL) {
            sixel_indexed_frame_unref(image);
            return SIXEL_BAD_ALLOCATION;
        }
    }
    *result = image;
    return SIXEL_OK;
}

SIXELAPI SIXELSTATUS
sixel_indexed_frame_new(sixel_indexed_frame_t **result,
    sixel_indexed_frame_description_t const *description,
    sixel_allocator_t *allocator)
{
    return sixel_indexed_frame_allocate(result, description, allocator, 1);
}

/* Validate only newly published rows. This also guards externally imported
 * data before ownership changes; a kept sentinel never indexes the palette. */
static SIXELSTATUS
sixel_indexed_pixels_validate(sixel_indexed_frame_description_t const *desc,
    sixel_index_t const *indexes, unsigned char const *skip,
    size_t first, size_t end)
{
    size_t i;
    unsigned int code;

    for (i = first; i < end; ++i) {
        code = skip != NULL ? skip[i] : SIXEL_INDEXED_PAINT;
        if (code != SIXEL_INDEXED_PAINT) {
            if (!desc->transparent_background ||
                    (code != SIXEL_INDEXED_KEEP &&
                     code != SIXEL_INDEXED_KEEP_APPROXIMATE)) {
                return SIXEL_BAD_ARGUMENT;
            }
        } else if ((size_t)indexes[i] >= desc->palette->view.count ||
                (int)indexes[i] == desc->keycolor) {
            return SIXEL_BAD_ARGUMENT;
        }
    }
    return SIXEL_OK;
}

SIXELAPI SIXELSTATUS
sixel_indexed_frame_take(sixel_indexed_frame_t **result,
    sixel_indexed_frame_take_request_t *request, sixel_allocator_t *allocator)
{
    size_t pixels;
    SIXELSTATUS status;

    if (request == NULL || result == NULL || *result != NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    status = sixel_indexed_description_validate(&request->description, &pixels);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if (request->indexes == NULL ||
            request->indexes_size != pixels * sizeof(*request->indexes) ||
            (request->skip != NULL && request->skip_size != pixels) ||
            (request->skip == NULL && request->skip_size != 0) ||
            (void *)request->indexes == (void *)request->skip) {
        return SIXEL_BAD_ARGUMENT;
    }
    status = sixel_indexed_pixels_validate(&request->description,
        request->indexes, request->skip, 0, pixels);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    status = sixel_indexed_frame_allocate(result, &request->description,
        allocator, 0);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    (*result)->indexes = request->indexes;
    (*result)->skip = request->skip;
    (*result)->identity = request->identity;
    sixel_atomic_store_relaxed_i32(&(*result)->rows,
        request->description.height);
    sixel_atomic_store_relaxed_i32(&(*result)->state, SIXEL_INDEXED_COMPLETE);
    request->indexes = NULL;
    request->skip = NULL;
    request->indexes_size = 0;
    request->skip_size = 0;
    return SIXEL_OK;
}

SIXELAPI void
sixel_indexed_frame_ref(sixel_indexed_frame_t *image)
{
    if (image != NULL) {
        (void)sixel_atomic_fetch_add_u32(&image->ref, 1);
    }
}

SIXELAPI void
sixel_indexed_frame_unref(sixel_indexed_frame_t *image)
{
    sixel_allocator_t *allocator;

    if (image != NULL &&
            sixel_atomic_fetch_sub_u32(&image->ref, 1) == 1) {
        allocator = image->allocator;
        sixel_wire_palette_unref(image->description.palette);
        sixel_allocator_free(allocator, image->indexes);
        sixel_allocator_free(allocator, image->skip);
        sixel_allocator_free(allocator, image);
        sixel_allocator_unref(allocator);
    }
}

SIXELAPI SIXELSTATUS
sixel_indexed_frame_get_description(sixel_indexed_frame_t const *image,
    sixel_indexed_frame_description_t *description)
{
    if (image == NULL || description == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *description = image->description;
    return SIXEL_OK;
}

SIXELAPI SIXELSTATUS
sixel_indexed_frame_get_identity(sixel_indexed_frame_t const *image,
    sixel_encode_identity_t *identity)
{
    if (image == NULL || identity == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *identity = image->identity;
    return SIXEL_OK;
}

SIXELAPI sixel_indexed_state_t
sixel_indexed_frame_get_state(sixel_indexed_frame_t const *image)
{
    int state;

    if (image == NULL) {
        return SIXEL_INDEXED_FAILED;
    }
    state = sixel_atomic_load_relaxed_i32(&image->state);
    sixel_fence_acquire();
    return (sixel_indexed_state_t)state;
}

SIXELAPI SIXELSTATUS
sixel_indexed_frame_get_rows(sixel_indexed_frame_t const *image,
    int first_row, int rows, sixel_indexed_rows_view_t *view)
{
    int ready;
    size_t offset;

    if (image == NULL || view == NULL || first_row < 0 || rows < 0 ||
            rows > image->description.height ||
            first_row > image->description.height - rows) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (sixel_indexed_frame_get_state(image) == SIXEL_INDEXED_FAILED) {
        return SIXEL_RUNTIME_ERROR;
    }
    ready = sixel_atomic_load_relaxed_i32(&image->rows);
    sixel_fence_acquire();
    if (first_row + rows > ready) {
        return SIXEL_FALSE;
    }
    offset = (size_t)first_row * (size_t)image->description.width;
    view->indexes = image->indexes + offset;
    view->skip = image->skip != NULL ? image->skip + offset : NULL;
    view->row_stride = (size_t)image->description.width;
    view->first_row = first_row;
    view->rows = rows;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_indexed_frame_publish_internal(sixel_indexed_frame_t *image, int rows,
    int applied)
{
    int previous;
    size_t i;
    size_t end;
    unsigned int code;
    SIXELSTATUS status;

    if (image == NULL || rows < 0 || rows > image->description.height) {
        return SIXEL_BAD_ARGUMENT;
    }
    previous = sixel_atomic_load_relaxed_i32(&image->rows);
    if (sixel_indexed_frame_get_state(image) != SIXEL_INDEXED_BUILDING ||
            rows < previous) {
        return SIXEL_LOGIC_ERROR;
    }
    status = SIXEL_OK;
    end = (size_t)rows * (size_t)image->description.width;
    for (i = (size_t)previous * image->description.width; i < end; ++i) {
        code = image->skip[i];
        /* A non-reserved transparent palette entry can be selected by the
         * legacy kernel. Normalize it before publication, in this same scan. */
        if (applied && (int)image->indexes[i] == image->description.keycolor &&
                code == SIXEL_INDEXED_PAINT) {
            image->skip[i] = code = SIXEL_INDEXED_KEEP;
        }
        if (code != SIXEL_INDEXED_PAINT) {
            if (!image->description.transparent_background ||
                    (code != SIXEL_INDEXED_KEEP &&
                     code != SIXEL_INDEXED_KEEP_APPROXIMATE)) {
                status = SIXEL_BAD_ARGUMENT;
                break;
            }
        } else if ((size_t)image->indexes[i] >=
                image->description.palette->view.count ||
                (int)image->indexes[i] == image->description.keycolor) {
            status = SIXEL_BAD_ARGUMENT;
            break;
        }
    }
    if (SIXEL_FAILED(status)) {
        sixel_indexed_frame_fail(image);
        return status;
    }
    sixel_fence_release();
    sixel_atomic_store_relaxed_i32(&image->rows, rows);
    return SIXEL_OK;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_indexed_frame_publish(sixel_indexed_frame_t *image, int rows)
{
    return sixel_indexed_frame_publish_internal(image, rows, 0);
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_indexed_frame_publish_applied(sixel_indexed_frame_t *image, int rows)
{
    return sixel_indexed_frame_publish_internal(image, rows, 1);
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_indexed_frame_publish_gpu(sixel_indexed_frame_t *image)
{
    if (image == NULL || sixel_indexed_frame_get_state(image) !=
            SIXEL_INDEXED_BUILDING ||
            sixel_atomic_load_relaxed_i32(&image->rows) != 0) {
        return SIXEL_LOGIC_ERROR;
    }
    /* The fixed kernel selected only validated palette entries and wrote
     * skip coverage with each keep. No second CPU image scan is needed. */
    sixel_fence_release();
    sixel_atomic_store_relaxed_i32(&image->rows, image->description.height);
    return SIXEL_OK;
}

SIXEL_INTERNAL_API void
sixel_indexed_frame_fail(sixel_indexed_frame_t *image)
{
    sixel_fence_release();
    sixel_atomic_store_relaxed_i32(&image->state, SIXEL_INDEXED_FAILED);
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_indexed_frame_complete(sixel_indexed_frame_t *image)
{
    if (sixel_indexed_frame_get_state(image) != SIXEL_INDEXED_BUILDING ||
            sixel_atomic_load_relaxed_i32(&image->rows) !=
                image->description.height) {
        return SIXEL_LOGIC_ERROR;
    }
    sixel_fence_release();
    sixel_atomic_store_relaxed_i32(&image->state, SIXEL_INDEXED_COMPLETE);
    return SIXEL_OK;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_stream_receipt_new(sixel_stream_receipt_t **result,
    sixel_encode_identity_t const *identity, sixel_allocator_t *allocator)
{
    sixel_stream_receipt_t *receipt;

    receipt = sixel_allocator_calloc(allocator, 1, sizeof(*receipt));
    if (receipt == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    receipt->ref = 1;
    receipt->allocator = allocator;
    if (identity != NULL) {
        receipt->identity = *identity;
    }
    sixel_allocator_ref(allocator);
    *result = receipt;
    return SIXEL_OK;
}

SIXELAPI void
sixel_stream_receipt_ref(sixel_stream_receipt_t *receipt)
{
    if (receipt != NULL) {
        (void)sixel_atomic_fetch_add_u32(&receipt->ref, 1);
    }
}

SIXELAPI void
sixel_stream_receipt_unref(sixel_stream_receipt_t *receipt)
{
    sixel_allocator_t *allocator;

    if (receipt != NULL &&
            sixel_atomic_fetch_sub_u32(&receipt->ref, 1) == 1) {
        allocator = receipt->allocator;
        sixel_allocator_free(allocator, receipt);
        sixel_allocator_unref(allocator);
    }
}

/* A session binds/unbinds updates only while users is zero. Resource users
 * keep the transaction alive even when its caller abandons the request. */
SIXEL_INTERNAL_API void
sixel_indexed_frame_acquire_user(sixel_indexed_frame_t *image)
{
    sixel_retained_update_ref(image->update);
    (void)sixel_atomic_fetch_add_u32(&image->users, 1);
}

SIXEL_INTERNAL_API void
sixel_indexed_frame_release_user(sixel_indexed_frame_t *image)
{
    sixel_retained_update_t *update;

    update = image->update;
    (void)sixel_atomic_fetch_sub_u32(&image->users, 1);
    sixel_retained_update_unref(update);
}
