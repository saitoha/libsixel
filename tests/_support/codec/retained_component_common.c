/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdlib.h>
#include "src/allocator.h"
#include "src/threading.h"
#include "indexed_component_common.h"
#include "retained_component_common.h"

static int retained_component_allocations;
static int retained_component_allow;

static void *
retained_component_calloc(size_t count, size_t size)
{
    ++retained_component_allocations;
    if (retained_component_allocations > retained_component_allow) return NULL;
    return calloc(count, size);
}

static void *
retained_component_malloc(size_t size)
{
    ++retained_component_allocations;
    if (retained_component_allocations > retained_component_allow) return NULL;
    return malloc(size);
}

static void *
retained_component_realloc(void *ptr, size_t size)
{
    ++retained_component_allocations;
    if (retained_component_allocations > retained_component_allow) return NULL;
    return realloc(ptr, size);
}

static int
retained_component_frame(indexed_fixture_t *f, int large)
{
    sixel_indexed_frame_description_t description;
    sixel_retained_plane_description_t plane;
    unsigned char *rgb;
    unsigned char *valid;

    description = f->image->description;
    if (large) {
        description.width = description.encoded_width = 1024;
        description.height = description.encoded_height = 1537;
        description.plane_width = 1026;
        description.plane_height = 1539;
        description.origin_y = 1;
    }
    sixel_retained_update_unref(f->update);
    f->update = NULL;
    sixel_stream_receipt_unref(f->receipt);
    f->receipt = NULL;
    sixel_indexed_frame_unref(f->image);
    f->image = NULL;
    if (sixel_indexed_frame_new(&f->image, &description, f->allocator) !=
            SIXEL_OK) return 0;
    memset(f->image->indexes, 0, f->image->pixels);
    memset(f->image->skip, 0, f->image->pixels);
    memset(&plane, 0, sizeof(plane));
    plane.width = description.plane_width;
    plane.height = description.plane_height;
    plane.color_definition = SIXEL_PALETTE_OUTPUT_LEGACY;
    plane.colorspace = SIXEL_COLORSPACE_GAMMA;
    if (f->plane->vtbl->configure(f->plane, &plane) != SIXEL_OK) return 0;
    if (!large) return 1;
    if (sixel_retained_plane_legacy_map(f->plane, &rgb, &valid) != SIXEL_OK) {
        return 0;
    }
    memset(rgb, 97, (size_t)plane.width * (size_t)plane.height * 3);
    memset(valid, 1, (size_t)plane.width * (size_t)plane.height);
    sixel_retained_plane_legacy_unmap(f->plane);
    return 1;
}

static SIXELSTATUS
retained_component_begin(indexed_fixture_t *f, int budget, int admission)
{
    sixel_retained_update_request_t request;
    sixel_calloc_t original;
    SIXELSTATUS status;

    status = f->plane->vtbl->acquire_read(f->plane, &f->read);
    if (status != SIXEL_OK) return status;
    status = sixel_retained_read_get_view(f->read, &f->view);
    if (status != SIXEL_OK) return status;
    memset(&request, 0, sizeof(request));
    request.baseline = f->read;
    request.image = f->image;
    request.sequence = f->view.sequence + 1;
    request.worker_threads = budget;
    original = f->allocator->fn_calloc;
    if (admission) {
        /* Admit the required update object, then deny optional worker storage.
         * No paint allocation is requested by this fixture. */
        retained_component_allow = 1;
        retained_component_allocations = 0;
        f->allocator->fn_calloc = retained_component_calloc;
    }
    status = f->plane->vtbl->begin_update(f->plane, &request, &f->update);
    f->allocator->fn_calloc = original;
    return status;
}

static SIXELSTATUS
retained_component_seal(indexed_fixture_t *f)
{
    sixel_writer_image_header_t header;
    SIXELSTATUS status;

    status = sixel_indexed_frame_publish(f->image,
        f->image->description.height);
    if (status != SIXEL_OK) return status;
    status = sixel_indexed_frame_complete(f->image);
    sixel_retained_read_unref(f->read);
    f->read = NULL;
    if (status != SIXEL_OK) return status;
    memset(&header, 0, sizeof(header));
    header.width = f->image->description.width;
    header.height = f->image->description.height;
    status = f->writer->vtbl->begin_image(f->writer, &header,
        &f->image->identity);
    if (status != SIXEL_OK) return status;
    return f->writer->vtbl->end_image(f->writer, &f->receipt);
}

int
retained_component_check(int large, int budget, int expected,
    enum retained_component_observation observation)
{
    indexed_fixture_t f;
    sixel_calloc_t original;
    sixel_malloc_t original_malloc;
    sixel_realloc_t original_realloc;
    size_t pixel;
    SIXELSTATUS status;
    int ok;

    memset(&f, 0, sizeof(f));
    ok = 0;
#if !SIXEL_ENABLE_THREADS
    if (expected > 1 || observation == RETAINED_COMPONENT_ADMISSION ||
            observation == RETAINED_COMPONENT_SHRINK) return 77;
#endif
    if (expected > 1 && sixel_get_hw_threads() < expected) return 77;
    if (observation == RETAINED_COMPONENT_SHRINK &&
            sixel_get_hw_threads() < budget) return 77;
    if (observation == RETAINED_COMPONENT_ADMISSION &&
            sixel_get_hw_threads() < 2) return 77;
    if (!indexed_fixture_init(&f) || !retained_component_frame(&f, large)) {
        goto end;
    }
    status = retained_component_begin(&f, budget,
        observation == RETAINED_COMPONENT_ADMISSION);
    if (observation == RETAINED_COMPONENT_NEGATIVE) {
        ok = status == SIXEL_BAD_ARGUMENT && f.update == NULL;
        goto end;
    }
    if (status != SIXEL_OK) goto end;
    if (observation == RETAINED_COMPONENT_ADMISSION &&
            retained_component_allocations < 2) goto end;
    if (observation == RETAINED_COMPONENT_ABORT) {
        pixel = (size_t)f.image->description.origin_y *
            (size_t)f.image->description.plane_width +
            (size_t)f.image->description.origin_x;
        ok = f.plane->vtbl->abort(f.plane, f.update) == SIXEL_OK &&
            memcmp(f.view.rgb + pixel * 3, "aaa", 3) == 0;
        goto end;
    }
    if (retained_component_seal(&f) != SIXEL_OK) goto end;
    if (observation == RETAINED_COMPONENT_WRONG_RECEIPT) {
        /* A malformed immutable receipt must be rejected before any worker
         * can write. Internal construction makes the negative fixture. */
        ++f.receipt->identity.sequence;
        status = indexed_fixture_commit(&f);
        if (status != SIXEL_LOGIC_ERROR ||
                f.plane->vtbl->acquire_read(f.plane, &f.read) != SIXEL_OK ||
                sixel_retained_read_get_view(f.read, &f.view) != SIXEL_OK) {
            goto end;
        }
        pixel = (size_t)f.image->description.origin_y *
            (size_t)f.image->description.plane_width +
            (size_t)f.image->description.origin_x;
        ok = memcmp(f.view.rgb + pixel * 3, "aaa", 3) == 0;
        goto end;
    }
    original = f.allocator->fn_calloc;
    original_malloc = f.allocator->fn_malloc;
    original_realloc = f.allocator->fn_realloc;
    if (observation == RETAINED_COMPONENT_NO_ALLOCATION) {
        retained_component_allow = 0;
        retained_component_allocations = 0;
        f.allocator->fn_calloc = retained_component_calloc;
        f.allocator->fn_malloc = retained_component_malloc;
        f.allocator->fn_realloc = retained_component_realloc;
    }
    status = indexed_fixture_commit(&f);
    f.allocator->fn_calloc = original;
    f.allocator->fn_malloc = original_malloc;
    f.allocator->fn_realloc = original_realloc;
    if (status != SIXEL_OK) goto end;
    if (observation == RETAINED_COMPONENT_NO_ALLOCATION) {
        ok = f.stats.worker_threads == expected &&
            retained_component_allocations == 0;
        goto end;
    }
    if (observation == RETAINED_COMPONENT_SHRINK ||
            observation == RETAINED_COMPONENT_GROW) {
        if (f.stats.worker_threads != budget ||
                !retained_component_frame(&f, large) ||
                retained_component_begin(&f, expected, 0) != SIXEL_OK ||
                retained_component_seal(&f) != SIXEL_OK ||
                indexed_fixture_commit(&f) != SIXEL_OK) goto end;
    }
    ok = f.stats.worker_threads == expected;
end:
    indexed_fixture_destroy(&f);
    return ok ? 0 : 1;
}
