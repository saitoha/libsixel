/* SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include <string.h>
#include "component_common.h"

typedef struct fixture_buffer {
    sixel_gpu_buffer_interface_t iface;
    size_t bytes;
    unsigned int refs;
} fixture_buffer_t;

static void
fixture_buffer_ref(sixel_gpu_buffer_interface_t *self)
{
    ++((fixture_buffer_t *)self)->refs;
}

static void
fixture_buffer_unref(sixel_gpu_buffer_interface_t *self)
{
    if (--((fixture_buffer_t *)self)->refs == 0) free(self);
}

static size_t
fixture_buffer_size(sixel_gpu_buffer_interface_t const *self)
{
    return ((fixture_buffer_t const *)self)->bytes;
}

static sixel_gpu_buffer_vtbl_t const fixture_buffer_vtbl = {
    fixture_buffer_ref, fixture_buffer_unref, fixture_buffer_size
};

static void
fixture_device_ref(sixel_gpu_device_interface_t *self)
{
    (void)self;
}

static char const *
fixture_format(sixel_gpu_device_interface_t const *self)
{
    (void)self;
    return "metal.lookup-rgb8.v1";
}

static SIXELSTATUS
fixture_allocate(sixel_gpu_device_interface_t *self, size_t bytes,
    sixel_gpu_buffer_interface_t **result)
{
    fixture_buffer_t *buffer;

    (void)self;
    buffer = calloc(1, sizeof(*buffer));
    if (buffer == NULL) return SIXEL_BAD_ALLOCATION;
    buffer->iface.vtbl = &fixture_buffer_vtbl;
    buffer->bytes = bytes;
    buffer->refs = 1;
    *result = &buffer->iface;
    return SIXEL_OK;
}

static sixel_gpu_device_vtbl_t const fixture_device_vtbl = {
    fixture_device_ref, fixture_device_ref, fixture_format, fixture_allocate
};

int
gpu_component_set_palette(gpu_component_fixture_t *fixture, unsigned char r)
{
    unsigned char colors[6];
    sixel_palette_entries_request_t entries;

    memset(colors, 0, sizeof(colors));
    colors[0] = r;
    colors[3] = 255;
    entries.entries = colors;
    entries.colors = 2;
    entries.depth = 3;
    return fixture->palette->vtbl->init_entries(fixture->palette, &entries)
        == SIXEL_OK;
}

int
gpu_component_open(gpu_component_fixture_t *fixture, int kind)
{
    void *object;
    SIXELSTATUS status;

    memset(fixture, 0, sizeof(*fixture));
    fixture->device.vtbl = fixture->other_device.vtbl = &fixture_device_vtbl;
    if (sixel_allocator_new(&fixture->allocator, NULL, NULL, NULL, NULL)
            != SIXEL_OK) return 0;
    object = NULL;
    if (sixel_palette_factory_new(fixture->allocator, &object) != SIXEL_OK) {
        return 0;
    }
    fixture->palette = object;
    if (!gpu_component_set_palette(fixture, 17)) return 0;
    switch (kind) {
    case 0:
        status = sixel_gpu_lookup_vptree_create(fixture->allocator,
            &fixture->provider);
        break;
    case 1:
        status = sixel_gpu_lookup_ordered_create(fixture->allocator,
            &fixture->provider);
        break;
    case 2:
        status = sixel_gpu_lookup_rbc_create(fixture->allocator,
            &fixture->provider);
        break;
    case 3:
        status = sixel_gpu_lookup_mahalanobis_create(fixture->allocator,
            &fixture->provider);
        break;
    default:
        status = sixel_gpu_lookup_certlut_create(fixture->allocator,
            &fixture->provider);
        break;
    }
    if (status != SIXEL_OK) return 0;
    fixture->request.working_palette = fixture->palette;
    fixture->request.ncolors = 2;
    fixture->request.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    fixture->request.colorspace = SIXEL_COLORSPACE_GAMMA;
    return fixture->provider->vtbl->prepare(fixture->provider,
        &fixture->request, &fixture->device, &fixture->plan) == SIXEL_OK;
}

void
gpu_component_close(gpu_component_fixture_t *fixture)
{
    if (fixture->second != NULL) {
        (void)fixture->second->vtbl->finish(fixture->second, SIXEL_RUNTIME_ERROR);
        fixture->second->vtbl->unref(fixture->second);
    }
    if (fixture->binding != NULL) {
        (void)fixture->binding->vtbl->finish(fixture->binding,
            SIXEL_RUNTIME_ERROR);
        fixture->binding->vtbl->unref(fixture->binding);
    }
    if (fixture->other != NULL) fixture->other->vtbl->unref(fixture->other);
    if (fixture->plan != NULL) fixture->plan->vtbl->unref(fixture->plan);
    if (fixture->provider != NULL) {
        fixture->provider->vtbl->unref(fixture->provider);
    }
    if (fixture->palette != NULL) fixture->palette->vtbl->unref(fixture->palette);
    sixel_allocator_unref(fixture->allocator);
}
