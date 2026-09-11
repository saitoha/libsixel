/*
 * SPDX-License-Identifier: MIT
 *
 * Format-neutral in-memory loader test support.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "tests/loader/pixelformat_test_common.h"
#include "src/cms.h"
#include "src/factory.h"
#include "src/loader.h"
#include "src/loader-common.h"
#include "loader_builtin_memory_test_common.h"

void
edge_put_u8(edge_writer_t *writer, unsigned int value)
{
    if (writer == NULL || writer->failed != 0) {
        return;
    }
    if (writer->length >= writer->capacity) {
        writer->failed = 1;
        return;
    }
    writer->buffer[writer->length++] = (unsigned char)value;
}

void
edge_put_u16le(edge_writer_t *writer, unsigned int value)
{
    edge_put_u8(writer, value & 0xffu);
    edge_put_u8(writer, (value >> 8) & 0xffu);
}

void
edge_put_u16be(edge_writer_t *writer, unsigned int value)
{
    edge_put_u8(writer, (value >> 8) & 0xffu);
    edge_put_u8(writer, value & 0xffu);
}

void
edge_put_bytes(edge_writer_t *writer,
               unsigned char const *bytes,
               size_t byte_count)
{
    if (writer == NULL || bytes == NULL || writer->failed != 0) {
        return;
    }
    if (writer->length > writer->capacity ||
        byte_count > writer->capacity - writer->length) {
        writer->failed = 1;
        return;
    }
    memcpy(writer->buffer + writer->length, bytes, byte_count);
    writer->length += byte_count;
}

static SIXELSTATUS
edge_capture_frame(sixel_frame_t *frame, void *data)
{
    edge_frame_probe_t *probe;
    unsigned char const *pixels;
    size_t rgb_size;
    int frame_index;
    int width;
    int height;
    int pixelformat;

    probe = (edge_frame_probe_t *)data;
    pixels = NULL;
    rgb_size = 0u;
    frame_index = 0;
    width = 0;
    height = 0;
    pixelformat = 0;
    if (frame == NULL || probe == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    frame_index = probe->callback_count;
    if (frame_index >= EDGE_FRAME_CAPACITY) {
        return SIXEL_BAD_INPUT;
    }

    width = sixel_frame_get_width(frame);
    height = sixel_frame_get_height(frame);
    pixelformat = sixel_frame_get_pixelformat(frame);
    probe->width[frame_index] = width;
    probe->height[frame_index] = height;
    probe->pixelformat[frame_index] = pixelformat;
    ++probe->callback_count;
    if (width <= 0 || height <= 0 ||
        pixelformat != SIXEL_PIXELFORMAT_RGB888 ||
        (size_t)width > SIZE_MAX / (size_t)height ||
        (size_t)width * (size_t)height > EDGE_RGB_CAPACITY / 3u) {
        return SIXEL_BAD_INPUT;
    }
    rgb_size = (size_t)width * (size_t)height * 3u;
    pixels = sixel_frame_get_pixels(frame);
    if (pixels == NULL) {
        return SIXEL_BAD_INPUT;
    }
    memcpy(probe->rgb[frame_index], pixels, rgb_size);
    probe->rgb_size[frame_index] = rgb_size;
    return SIXEL_OK;
}

int
edge_load_buffer(char const *label,
                 unsigned char const *buffer,
                 size_t buffer_size,
                 int require_static,
                 edge_frame_probe_t *probe,
                 SIXELSTATUS *load_status)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_loader_component_t *component;
    sixel_chunk_t *chunk;
    loader_probe_callback_state_t callback_state;
    int use_palette;
    int reqcolors;
    int loop_control;
    int cms_engine;
    int result;

    status = SIXEL_FALSE;
    allocator = NULL;
    component = NULL;
    chunk = NULL;
    use_palette = 0;
    reqcolors = 256;
    loop_control = SIXEL_LOOP_DISABLE;
    cms_engine = SIXEL_CMS_ENGINE_NONE;
    result = 1;
    if (load_status != NULL) {
        *load_status = SIXEL_FALSE;
    }
    if (label == NULL || buffer == NULL || buffer_size == 0u ||
        probe == NULL || load_status == NULL) {
        return 1;
    }
    memset(probe, 0, sizeof(*probe));

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: allocator initialization failed\n", label);
        return 1;
    }
    status = create_loader_component_by_name("builtin",
                                             allocator,
                                             (void **)&component);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: builtin component creation failed\n", label);
        goto cleanup;
    }
    status = sixel_chunk_create_from_memory(&chunk,
                                            buffer,
                                            buffer_size,
                                            NULL,
                                            allocator);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: memory chunk creation failed\n", label);
        goto cleanup;
    }
    status = sixel_loader_component_setopt(component,
                                           SIXEL_LOADER_OPTION_REQUIRE_STATIC,
                                           &require_static);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_loader_component_setopt(component,
                                           SIXEL_LOADER_OPTION_USE_PALETTE,
                                           &use_palette);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_loader_component_setopt(component,
                                           SIXEL_LOADER_OPTION_REQCOLORS,
                                           &reqcolors);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_loader_component_setopt(component,
                                           SIXEL_LOADER_OPTION_LOOP_CONTROL,
                                           &loop_control);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_loader_component_setopt(
        component,
        SIXEL_LOADER_COMPONENT_OPTION_CMS_ENGINE,
        &cms_engine);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    callback_state.loader = NULL;
    callback_state.fn = edge_capture_frame;
    callback_state.context = probe;
    *load_status = sixel_loader_component_load(component,
                                               chunk,
                                               capture_frame_trampoline,
                                               &callback_state);
    result = 0;

cleanup:
    sixel_loader_component_unref(component);
    if (chunk != NULL) {
        chunk->vtbl->unref(chunk);
    }
    sixel_allocator_unref(allocator);
    return result;
}

int
edge_expect_rgb(char const *label,
                unsigned char const *buffer,
                size_t buffer_size,
                int require_static,
                int expected_width,
                int expected_height,
                int expected_frames,
                unsigned char const *expected_rgb,
                size_t expected_frame_size)
{
    edge_frame_probe_t probe;
    SIXELSTATUS status;
    int frame_index;
    int result;

    memset(&probe, 0, sizeof(probe));
    status = SIXEL_FALSE;
    frame_index = 0;
    result = edge_load_buffer(label,
                              buffer,
                              buffer_size,
                              require_static,
                              &probe,
                              &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: loader failed (%d)\n", label, (int)status);
        return 1;
    }
    if (probe.callback_count != expected_frames) {
        fprintf(stderr,
                "%s: callback count mismatch (%d, expected %d)\n",
                label,
                probe.callback_count,
                expected_frames);
        return 1;
    }
    for (frame_index = 0;
         frame_index < expected_frames;
         ++frame_index) {
        if (probe.width[frame_index] != expected_width ||
            probe.height[frame_index] != expected_height ||
            probe.pixelformat[frame_index] != SIXEL_PIXELFORMAT_RGB888 ||
            probe.rgb_size[frame_index] != expected_frame_size) {
            fprintf(stderr, "%s: frame %d metadata mismatch\n",
                    label, frame_index);
            return 1;
        }
        if (expected_rgb != NULL &&
            memcmp(probe.rgb[frame_index],
                   expected_rgb + (size_t)frame_index * expected_frame_size,
                   expected_frame_size) != 0) {
            fprintf(stderr, "%s: frame %d RGB mismatch\n",
                    label, frame_index);
            return 1;
        }
    }
    return 0;
}

int
edge_expect_failure(char const *label,
                    unsigned char const *buffer,
                    size_t buffer_size)
{
    edge_frame_probe_t probe;
    SIXELSTATUS status;
    int result;

    memset(&probe, 0, sizeof(probe));
    status = SIXEL_FALSE;
    result = edge_load_buffer(label,
                              buffer,
                              buffer_size,
                              1,
                              &probe,
                              &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_SUCCEEDED(status)) {
        fprintf(stderr, "%s: malformed stream unexpectedly succeeded\n",
                label);
        return 1;
    }
    if (probe.callback_count != 0) {
        fprintf(stderr, "%s: malformed stream emitted a frame\n", label);
        return 1;
    }
    return 0;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
