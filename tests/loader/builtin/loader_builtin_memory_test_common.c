/*
 * SPDX-License-Identifier: MIT
 *
 * Format-neutral in-memory loader test support.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <6cells.h>

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

void
edge_loader_options_init(edge_loader_options_t *options)
{
    if (options == NULL) {
        return;
    }
    memset(options, 0, sizeof(*options));
    options->reqcolors = 256;
    options->loop_control = SIXEL_LOOP_DISABLE;
    options->cms_engine = SIXEL_CMS_ENGINE_NONE;
}

static SIXELSTATUS
edge_capture_frame(sixel_frame_t *frame, void *data)
{
    SIXELSTATUS status;
    sixel_frame_interface_t *frame_if;
    sixel_frame_transparency_t transparency;
    edge_frame_probe_t *probe;
    unsigned char const *pixels;
    unsigned char const *palette;
    size_t rgb_size;
    size_t palette_size;
    int frame_index;
    int width;
    int height;
    int pixelformat;
    int depth;
    int ncolors;

    status = SIXEL_FALSE;
    frame_if = NULL;
    memset(&transparency, 0, sizeof(transparency));
    probe = (edge_frame_probe_t *)data;
    pixels = NULL;
    palette = NULL;
    rgb_size = 0u;
    palette_size = 0u;
    frame_index = 0;
    width = 0;
    height = 0;
    pixelformat = 0;
    depth = 0;
    ncolors = 0;
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
    depth = sixel_helper_compute_depth(pixelformat);
    ncolors = sixel_frame_get_ncolors(frame);
    probe->width[frame_index] = width;
    probe->height[frame_index] = height;
    probe->pixelformat[frame_index] = pixelformat;
    probe->colorspace[frame_index] = sixel_frame_get_colorspace(frame);
    probe->ncolors[frame_index] = ncolors;
    probe->delay[frame_index] = sixel_frame_get_delay(frame);
    probe->frame_no[frame_index] = sixel_frame_get_frame_no(frame);
    probe->loop_no[frame_index] = sixel_frame_get_loop_no(frame);
    probe->multiframe[frame_index] = sixel_frame_get_multiframe(frame);
    probe->transparent[frame_index] = sixel_frame_get_transparent(frame);
    ++probe->callback_count;
    if (width <= 0 || height <= 0 ||
        depth <= 0 ||
        (size_t)width > SIZE_MAX / (size_t)height ||
        (size_t)width * (size_t)height >
            EDGE_RGB_CAPACITY / (size_t)depth) {
        return SIXEL_BAD_INPUT;
    }
    rgb_size = (size_t)width * (size_t)height * (size_t)depth;
    pixels = sixel_frame_get_pixels(frame);
    if (pixels == NULL) {
        return SIXEL_BAD_INPUT;
    }
    memcpy(probe->rgb[frame_index], pixels, rgb_size);
    probe->rgb_size[frame_index] = rgb_size;

    palette = sixel_frame_get_palette(frame);
    if (palette != NULL && ncolors > 0) {
        if ((size_t)ncolors > EDGE_PALETTE_CAPACITY / 3u) {
            return SIXEL_BAD_INPUT;
        }
        palette_size = (size_t)ncolors * 3u;
        memcpy(probe->palette[frame_index], palette, palette_size);
        probe->palette_size[frame_index] = palette_size;
    }

    frame_if = sixel_frame_as_interface(frame);
    if (frame_if == NULL || frame_if->vtbl == NULL ||
        frame_if->vtbl->get_transparency == NULL) {
        return SIXEL_BAD_INPUT;
    }
    status = frame_if->vtbl->get_transparency(frame_if, &transparency);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    probe->alpha_zero_is_transparent[frame_index] =
        transparency.alpha_zero_is_transparent;
    if (transparency.transparent_mask != NULL) {
        if (transparency.transparent_mask_size > EDGE_MASK_CAPACITY) {
            return SIXEL_BAD_INPUT;
        }
        memcpy(probe->mask[frame_index],
               transparency.transparent_mask,
               transparency.transparent_mask_size);
        probe->mask_size[frame_index] =
            transparency.transparent_mask_size;
    }
    return SIXEL_OK;
}

static int
edge_load_chunk(char const *label,
                sixel_chunk_t *chunk,
                edge_loader_options_t const *options,
                edge_frame_probe_t *probe,
                SIXELSTATUS *load_status)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_loader_component_t *component;
    loader_probe_callback_state_t callback_state;
    int result;

    status = SIXEL_FALSE;
    allocator = NULL;
    component = NULL;
    result = 1;
    if (load_status != NULL) {
        *load_status = SIXEL_FALSE;
    }
    if (label == NULL || chunk == NULL || options == NULL || probe == NULL ||
        load_status == NULL) {
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
    status = sixel_loader_component_setopt(component,
                                           SIXEL_LOADER_OPTION_REQUIRE_STATIC,
                                           &options->require_static);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_loader_component_setopt(component,
                                           SIXEL_LOADER_OPTION_USE_PALETTE,
                                           &options->use_palette);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_loader_component_setopt(component,
                                           SIXEL_LOADER_OPTION_REQCOLORS,
                                           &options->reqcolors);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_loader_component_setopt(component,
                                           SIXEL_LOADER_OPTION_LOOP_CONTROL,
                                           &options->loop_control);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_loader_component_setopt(
        component,
        SIXEL_LOADER_COMPONENT_OPTION_CMS_ENGINE,
        &options->cms_engine);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    if (options->set_bgcolor != 0) {
        status = sixel_loader_component_setopt(component,
                                               SIXEL_LOADER_OPTION_BGCOLOR,
                                               options->bgcolor);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
    }
    if (options->set_prefer_float32 != 0) {
        status = sixel_loader_component_setopt(
            component,
            SIXEL_LOADER_COMPONENT_OPTION_PREFER_FLOAT32,
            &options->prefer_float32);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
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
    sixel_allocator_unref(allocator);
    return result;
}

int
edge_load_buffer_options(char const *label,
                         unsigned char const *buffer,
                         size_t buffer_size,
                         edge_loader_options_t const *options,
                         edge_frame_probe_t *probe,
                         SIXELSTATUS *load_status)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_chunk_t *chunk;
    int result;

    status = SIXEL_FALSE;
    allocator = NULL;
    chunk = NULL;
    result = 1;
    if (label == NULL || buffer == NULL || buffer_size == 0u ||
        options == NULL || probe == NULL || load_status == NULL) {
        return 1;
    }
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: allocator initialization failed\n", label);
        return 1;
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
    result = edge_load_chunk(label,
                             chunk,
                             options,
                             probe,
                             load_status);

cleanup:
    if (chunk != NULL) {
        chunk->vtbl->unref(chunk);
    }
    sixel_allocator_unref(allocator);
    return result;
}

int
edge_load_buffer(char const *label,
                 unsigned char const *buffer,
                 size_t buffer_size,
                 int require_static,
                 edge_frame_probe_t *probe,
                 SIXELSTATUS *load_status)
{
    edge_loader_options_t options;

    edge_loader_options_init(&options);
    options.require_static = require_static;
    return edge_load_buffer_options(label,
                                    buffer,
                                    buffer_size,
                                    &options,
                                    probe,
                                    load_status);
}

int
edge_load_fixture_options(char const *label,
                          char const *relative_path,
                          edge_loader_options_t const *options,
                          edge_frame_probe_t *probe,
                          SIXELSTATUS *load_status)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_chunk_t *chunk;
    char const *source_root;
    char image_path[PATH_MAX];
    int cancel_flag;
    int result;

    status = SIXEL_FALSE;
    allocator = NULL;
    chunk = NULL;
    source_root = NULL;
    cancel_flag = 0;
    result = 1;
    if (label == NULL || relative_path == NULL || options == NULL ||
        probe == NULL ||
        load_status == NULL) {
        return 1;
    }
    source_root = sixel_compat_getenv("MESON_SOURCE_ROOT");
    if (source_root == NULL) {
        source_root = sixel_compat_getenv("abs_top_srcdir");
    }
    if (source_root == NULL) {
        source_root = sixel_compat_getenv("TOP_SRCDIR");
    }
    if (source_root == NULL) {
        source_root = ".";
    }
    if (build_image_path(source_root,
                         relative_path,
                         image_path,
                         sizeof(image_path)) != 0) {
        fprintf(stderr, "%s: failed to build fixture path\n", label);
        return 1;
    }
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: allocator initialization failed\n", label);
        return 1;
    }
    status = sixel_chunk_create_from_source(&chunk,
                                            image_path,
                                            0,
                                            &cancel_flag,
                                            allocator);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: fixture read failed\n", label);
        goto cleanup;
    }
    result = edge_load_chunk(label,
                             chunk,
                             options,
                             probe,
                             load_status);

cleanup:
    if (chunk != NULL) {
        chunk->vtbl->unref(chunk);
    }
    sixel_allocator_unref(allocator);
    return result;
}

int
edge_load_fixture(char const *label,
                  char const *relative_path,
                  int require_static,
                  edge_frame_probe_t *probe,
                  SIXELSTATUS *load_status)
{
    edge_loader_options_t options;

    edge_loader_options_init(&options);
    options.require_static = require_static;
    return edge_load_fixture_options(label,
                                     relative_path,
                                     &options,
                                     probe,
                                     load_status);
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

uint64_t
edge_digest_bytes(unsigned char const *bytes, size_t byte_count)
{
    uint64_t digest;
    size_t index;

    digest = UINT64_C(14695981039346656037);
    for (index = 0u; index < byte_count; ++index) {
        digest ^= (uint64_t)bytes[index];
        digest *= UINT64_C(1099511628211);
    }
    return digest;
}

int
edge_expect_fixture_digests_options(
    char const *label,
    char const *relative_path,
    edge_loader_options_t const *options,
    int expected_width,
    int expected_height,
    int expected_frames,
    int expected_pixelformat,
    int expected_colorspace,
    uint64_t const *expected_digests)
{
    edge_frame_probe_t probe;
    SIXELSTATUS status;
    uint64_t actual_digest;
    size_t expected_size;
    int frame_index;
    int expected_depth;
    int result;

    memset(&probe, 0, sizeof(probe));
    status = SIXEL_FALSE;
    actual_digest = 0u;
    expected_size = 0u;
    frame_index = 0;
    expected_depth = sixel_helper_compute_depth(expected_pixelformat);
    if (expected_width <= 0 || expected_height <= 0 ||
        expected_depth <= 0) {
        return 1;
    }
    expected_size = (size_t)expected_width * (size_t)expected_height *
                    (size_t)expected_depth;
    result = edge_load_fixture_options(label,
                                       relative_path,
                                       options,
                                       &probe,
                                       &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: loader failed (%d)\n", label, (int)status);
        return 1;
    }
    if (expected_digests == NULL || expected_width <= 0 ||
        expected_height <= 0 || expected_frames <= 0 ||
        expected_frames > EDGE_FRAME_CAPACITY) {
        fprintf(stderr, "%s: invalid digest expectation\n", label);
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
            probe.pixelformat[frame_index] != expected_pixelformat ||
            probe.colorspace[frame_index] != expected_colorspace ||
            probe.rgb_size[frame_index] != expected_size) {
            fprintf(stderr,
                    "%s: frame %d metadata %dx%d pf=%d cs=%d size=%lu, "
                    "expected %dx%d pf=%d cs=%d size=%lu\n",
                    label,
                    frame_index,
                    probe.width[frame_index],
                    probe.height[frame_index],
                    probe.pixelformat[frame_index],
                    probe.colorspace[frame_index],
                    (unsigned long)probe.rgb_size[frame_index],
                    expected_width,
                    expected_height,
                    expected_pixelformat,
                    expected_colorspace,
                    (unsigned long)expected_size);
            return 1;
        }
        actual_digest = edge_digest_bytes(probe.rgb[frame_index],
                                          probe.rgb_size[frame_index]);
        if (actual_digest != expected_digests[frame_index]) {
            fprintf(stderr,
                    "%s: frame %d RGB digest 0x%016llx, "
                    "expected 0x%016llx\n",
                    label,
                    frame_index,
                    (unsigned long long)actual_digest,
                    (unsigned long long)expected_digests[frame_index]);
            return 1;
        }
    }
    return 0;
}

int
edge_expect_fixture_rgb_digests(char const *label,
                                char const *relative_path,
                                int require_static,
                                int expected_width,
                                int expected_height,
                                int expected_frames,
                                uint64_t const *expected_digests)
{
    edge_loader_options_t options;

    edge_loader_options_init(&options);
    options.require_static = require_static;
    return edge_expect_fixture_digests_options(
        label,
        relative_path,
        &options,
        expected_width,
        expected_height,
        expected_frames,
        SIXEL_PIXELFORMAT_RGB888,
        SIXEL_COLORSPACE_GAMMA,
        expected_digests);
}

int
edge_expect_fixture_float_samples(
    char const *label,
    char const *relative_path,
    edge_loader_options_t const *options,
    int expected_width,
    int expected_height,
    int expected_pixelformat,
    int expected_colorspace,
    size_t const sample_pixels[3],
    float const expected_samples[9],
    float tolerance)
{
    edge_frame_probe_t probe;
    SIXELSTATUS status;
    float actual_samples[9];
    float delta;
    size_t pixel_count;
    size_t sample_index;
    size_t component_index;
    size_t output_index;
    int result;

    memset(&probe, 0, sizeof(probe));
    status = SIXEL_FALSE;
    memset(actual_samples, 0, sizeof(actual_samples));
    delta = 0.0f;
    pixel_count = 0u;
    sample_index = 0u;
    component_index = 0u;
    output_index = 0u;
    result = edge_load_fixture_options(label,
                                       relative_path,
                                       options,
                                       &probe,
                                       &status);
    if (result != 0 || SIXEL_FAILED(status)) {
        return 1;
    }
    if (sample_pixels == NULL || expected_samples == NULL ||
        expected_width <= 0 || expected_height <= 0 || tolerance < 0.0f ||
        !SIXEL_PIXELFORMAT_IS_FLOAT32(expected_pixelformat)) {
        return 1;
    }
    pixel_count = (size_t)expected_width * (size_t)expected_height;
    if (probe.callback_count != 1 || probe.width[0] != expected_width ||
        probe.height[0] != expected_height ||
        probe.pixelformat[0] != expected_pixelformat ||
        probe.colorspace[0] != expected_colorspace ||
        probe.rgb_size[0] != pixel_count * 3u * sizeof(float) ||
        sample_pixels[0] >= pixel_count ||
        sample_pixels[1] >= pixel_count ||
        sample_pixels[2] >= pixel_count) {
        fprintf(stderr, "%s: float frame metadata mismatch\n", label);
        return 1;
    }
    for (sample_index = 0u; sample_index < 3u; ++sample_index) {
        for (component_index = 0u;
             component_index < 3u;
             ++component_index) {
            output_index = sample_index * 3u + component_index;
            memcpy(actual_samples + output_index,
                   probe.rgb[0] +
                       (sample_pixels[sample_index] * 3u +
                        component_index) * sizeof(float),
                   sizeof(float));
        }
    }
    for (output_index = 0u; output_index < 9u; ++output_index) {
        delta = actual_samples[output_index] -
                expected_samples[output_index];
        if (actual_samples[output_index] != actual_samples[output_index]) {
            return 1;
        }
        if (delta < 0.0f) {
            delta = -delta;
        }
        if (delta > tolerance) {
            fprintf(stderr,
                    "%s: samples %.9g %.9g %.9g / "
                    "%.9g %.9g %.9g / %.9g %.9g %.9g\n",
                    label,
                    actual_samples[0],
                    actual_samples[1],
                    actual_samples[2],
                    actual_samples[3],
                    actual_samples[4],
                    actual_samples[5],
                    actual_samples[6],
                    actual_samples[7],
                    actual_samples[8]);
            return 1;
        }
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
