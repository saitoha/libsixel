/* SPDX-License-Identifier: MIT */
#include "indexed_component_common.h"

static int
indexed_fixture_write(char *bytes, int size, void *context)
{
    indexed_fixture_t *f;

    (void)bytes;
    f = context;
    ++f->calls;
    return f->reject ? -1 : size;
}

int
indexed_fixture_init(indexed_fixture_t *f)
{
    sixel_wire_color_definition_t definitions[3] = {
        {0, 2, {100, 0, 0}}, {1, 2, {0, 100, 0}}, {2, 2, {0, 0, 0}}
    };
    sixel_wire_palette_request_t wire;
    sixel_indexed_frame_description_t image;
    sixel_retained_plane_description_t plane;
    sixel_writer_init_request_t writer;
    unsigned char *rgb;
    unsigned char *valid;
    void *object;

    memset(f, 0, sizeof(*f));
    memset(&wire, 0, sizeof(wire));
    memset(&image, 0, sizeof(image));
    memset(&plane, 0, sizeof(plane));
    if (sixel_allocator_new(&f->allocator, NULL, NULL, NULL, NULL) !=
            SIXEL_OK) return 0;
    wire.definitions = definitions;
    wire.count = 3;
    wire.color_definition = SIXEL_PALETTE_OUTPUT_LEGACY;
    wire.colorspace = SIXEL_COLORSPACE_GAMMA;
    if (sixel_wire_palette_new(&f->wire, &wire, f->allocator) != SIXEL_OK) {
        return 0;
    }
    image.width = image.encoded_width = 2;
    image.height = image.encoded_height = 1;
    image.plane_width = 3;
    image.plane_height = 1;
    image.origin_x = 1;
    image.palette = f->wire;
    image.keycolor = 2;
    image.keycolor_reserved = 1;
    image.transparent_background = 1;
    if (sixel_indexed_frame_new(&f->image, &image, f->allocator) != SIXEL_OK) {
        return 0;
    }
    f->image->indexes[0] = 0;
    f->image->indexes[1] = (sixel_index_t)-1;
    f->image->skip[0] = SIXEL_INDEXED_PAINT;
    f->image->skip[1] = SIXEL_INDEXED_KEEP_APPROXIMATE;
    object = NULL;
    if (sixel_retained_plane_factory_new(f->allocator, &object) != SIXEL_OK) {
        return 0;
    }
    f->plane = object;
    plane.width = 3;
    plane.height = 1;
    plane.color_definition = SIXEL_PALETTE_OUTPUT_LEGACY;
    plane.colorspace = SIXEL_COLORSPACE_GAMMA;
    if (f->plane->vtbl->configure(f->plane, &plane) != SIXEL_OK ||
            sixel_retained_plane_legacy_map(f->plane, &rgb, &valid) !=
                SIXEL_OK) return 0;
    memcpy(rgb, "\12\24\36\12\24\36\12\24\36", 9);
    memset(valid, 1, 3);
    sixel_retained_plane_legacy_unmap(f->plane);
    object = NULL;
    if (sixel_writer_factory_new(f->allocator, &object) != SIXEL_OK) return 0;
    f->writer = object;
    writer.fn_write = indexed_fixture_write;
    writer.priv = f;
    return f->writer->vtbl->init(f->writer, &writer) == SIXEL_OK;
}

SIXELSTATUS
indexed_fixture_begin(indexed_fixture_t *f)
{
    sixel_retained_update_request_t request;
    SIXELSTATUS status;

    status = f->plane->vtbl->acquire_read(f->plane, &f->read);
    if (status != SIXEL_OK) return status;
    status = sixel_retained_read_get_view(f->read, &f->view);
    if (status != SIXEL_OK) return status;
    memset(&request, 0, sizeof(request));
    request.baseline = f->read;
    request.image = f->image;
    request.sequence = f->view.sequence + 1;
    request.record_paint = 1;
    return f->plane->vtbl->begin_update(f->plane, &request, &f->update);
}

SIXELSTATUS
indexed_fixture_seal(indexed_fixture_t *f)
{
    SIXELSTATUS status;

    status = sixel_indexed_frame_publish(f->image, 1);
    if (status != SIXEL_OK) return status;
    status = sixel_indexed_frame_complete(f->image);
    sixel_retained_read_unref(f->read);
    f->read = NULL;
    return status;
}

SIXELSTATUS
indexed_fixture_accept(indexed_fixture_t *f)
{
    sixel_writer_image_header_t header;
    SIXELSTATUS status;

    memset(&header, 0, sizeof(header));
    header.width = 2;
    header.height = 1;
    status = f->writer->vtbl->begin_image(f->writer, &header,
        &f->image->identity);
    if (status != SIXEL_OK) return status;
    return f->writer->vtbl->end_image(f->writer, &f->receipt);
}

SIXELSTATUS
indexed_fixture_commit(indexed_fixture_t *f)
{
    return f->plane->vtbl->commit(f->plane, f->update, f->receipt, &f->stats);
}

void
indexed_fixture_destroy(indexed_fixture_t *f)
{
    sixel_stream_receipt_unref(f->receipt);
    if (f->writer != NULL) f->writer->vtbl->unref(f->writer);
    sixel_retained_read_unref(f->read);
    sixel_retained_update_unref(f->update);
    sixel_indexed_frame_unref(f->image);
    if (f->plane != NULL) f->plane->vtbl->unref(f->plane);
    sixel_wire_palette_unref(f->wire);
    sixel_allocator_unref(f->allocator);
}
