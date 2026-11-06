/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <string.h>
#include "indexed_component_common.h"
#include "src/gpu-palette.h"

static SIXELSTATUS
retained_gpu_fixture_begin(indexed_fixture_t *f, int paint, int resolved)
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
    request.record_paint = paint;
    if (resolved)
        return f->plane->vtbl->begin_resolved_update(f->plane, &request,
            &f->update);
    return f->plane->vtbl->begin_update(f->plane, &request, &f->update);
}

static int
retained_gpu_fixture_resize(indexed_fixture_t *f, int partial, int statistics)
{
    sixel_indexed_frame_description_t image;
    sixel_retained_plane_description_t plane;
    unsigned char *rgb;
    unsigned char *valid;
    size_t i;

    image = f->image->description;
    image.width = image.encoded_width = statistics ? 17 : 3;
    image.height = image.encoded_height = statistics ? 17 : 5;
    image.origin_x = partial ? 2 : 0;
    image.origin_y = partial ? 2 : 0;
    image.plane_width = partial ? 7 : image.width;
    image.plane_height = partial ? 8 : image.height;
    sixel_indexed_frame_unref(f->image);
    f->image = NULL;
    if (sixel_indexed_frame_new(&f->image, &image, f->allocator) != SIXEL_OK)
        return 0;
    for (i = 0; i < f->image->pixels; ++i) {
        f->image->indexes[i] = (sixel_index_t)(i % 2);
        f->image->skip[i] = 0;
    }
    memset(&plane, 0, sizeof(plane));
    plane.width = image.plane_width;
    plane.height = image.plane_height;
    plane.color_definition = SIXEL_PALETTE_OUTPUT_LEGACY;
    plane.colorspace = SIXEL_COLORSPACE_GAMMA;
    if (f->plane->vtbl->configure(f->plane, &plane) != SIXEL_OK ||
            sixel_retained_plane_legacy_map(f->plane, &rgb, &valid) != SIXEL_OK)
        return 0;
    memset(rgb, 17, (size_t)plane.width * plane.height * 3);
    memset(valid, 0, (size_t)plane.width * plane.height);
    sixel_retained_plane_legacy_unmap(f->plane);
    return 1;
}

static int
retained_gpu_observe_impl(int observation, int resolved)
{
    indexed_fixture_t f;
    sixel_gpu_device_interface_t *device;
    sixel_retained_gpu_read_view_t gpu;
    sixel_retained_gpu_read_view_t previous;
    sixel_retained_read_view_t before;
    unsigned char *rgb;
    unsigned char *valid;
    sixel_encode_identity_t identity;
    SIXELSTATUS status;
    int ok;
    int x;
    int y;
    size_t pixel;
    size_t local;
    int inside;

    device = NULL;
    memset(&f, 0, sizeof(f));
    ok = 0;
    status = sixel_gpu_device_acquire(&device);
    if (status == SIXEL_FEATURE_ERROR) return 77;
    if (status != SIXEL_OK || !indexed_fixture_init(&f)) goto end;
    if ((observation == 8 || observation == 19 || observation == 20 ||
            observation == 21) && !retained_gpu_fixture_resize(&f,
                observation == 21, observation == 8 || observation == 19))
        goto end;
    /* A 17x17 image spans a complete 256-thread group and a short tail.
     * Put the only approximate keep late in the first group: truncating its
     * SIMD partial results must fail even if the last threadgroup is read. */
    if (observation == 19) {
        memset(f.image->skip, SIXEL_INDEXED_KEEP, f.image->pixels);
        f.image->skip[255] = SIXEL_INDEXED_KEEP_APPROXIMATE;
    }
    if (f.plane->vtbl->set_device(f.plane, device) != SIXEL_OK) goto end;
    if (f.plane->vtbl->acquire_read(f.plane, &f.read) != SIXEL_OK ||
            sixel_retained_read_get_view(f.read, &before) != SIXEL_OK ||
            sixel_retained_read_get_gpu_view(f.read, device, &previous) !=
                SIXEL_OK) goto end;
    if (observation == 0) {
        ok = memcmp(before.rgb, "\12\24\36\12\24\36\12\24\36", 9) == 0;
        goto end;
    }
    if (observation == 2) {
        ok = f.plane->vtbl->set_device(f.plane, NULL) == SIXEL_LOGIC_ERROR;
        goto end;
    }
    sixel_retained_read_unref(f.read);
    f.read = NULL;
    if (observation == 1) {
        if (f.plane->vtbl->set_device(f.plane, device) != SIXEL_OK ||
                f.plane->vtbl->acquire_read(f.plane, &f.read) != SIXEL_OK ||
                sixel_retained_read_get_gpu_view(f.read, device, &gpu) !=
                    SIXEL_OK) goto end;
        ok = gpu.rgb == previous.rgb && gpu.valid == previous.valid;
        goto end;
    }
    if (observation == 18) {
        if (sixel_retained_plane_legacy_map(f.plane, &rgb, &valid) != SIXEL_OK)
            goto end;
        rgb[0] = 79;
        valid[0] = 0;
        sixel_retained_plane_legacy_unmap(f.plane);
        if (f.plane->vtbl->acquire_read(f.plane, &f.read) != SIXEL_OK ||
                sixel_retained_read_get_view(f.read, &f.view) != SIXEL_OK)
            goto end;
        ok = f.view.rgb[0] == 79 && f.view.valid[0] == 0;
        goto end;
    }
    if (observation == 22) {
        memset(f.image->skip, SIXEL_INDEXED_KEEP_APPROXIMATE, f.image->pixels);
        memset(f.image->indexes, 255, f.image->pixels);
    }
    if (retained_gpu_fixture_begin(&f, observation != 10, resolved) != SIXEL_OK)
        goto end;
    if (observation == 3) {
        sixel_retained_read_unref(f.read);
        f.read = NULL;
        ok = f.plane->vtbl->set_device(f.plane, NULL) == SIXEL_LOGIC_ERROR;
        goto end;
    }
    if (observation == 4) {
        ok = f.plane->vtbl->stage(f.plane, f.update) == SIXEL_LOGIC_ERROR;
        goto end;
    }
    if (sixel_indexed_frame_publish(f.image, f.image->description.height) !=
            SIXEL_OK || sixel_indexed_frame_complete(f.image) != SIXEL_OK)
        goto end;
    if (observation == 5) {
        if (indexed_fixture_accept(&f) != SIXEL_OK) goto end;
        sixel_retained_read_unref(f.read);
        f.read = NULL;
        ok = indexed_fixture_commit(&f) == SIXEL_LOGIC_ERROR;
        goto end;
    }
    if (observation == 15 || observation == 16) {
        if (sixel_retained_plane_test_gpu_fault(f.plane, observation - 14) !=
                SIXEL_OK) goto end;
        status = f.plane->vtbl->stage(f.plane, f.update);
        ok = status == SIXEL_RUNTIME_ERROR &&
            memcmp(f.view.rgb, "\12\24\36\12\24\36\12\24\36", 9) == 0;
        goto end;
    }
    if (f.plane->vtbl->stage(f.plane, f.update) != SIXEL_OK) goto end;
    if (observation == 6) {
        ok = memcmp(f.view.rgb, "\12\24\36\12\24\36\12\24\36", 9) == 0;
        goto end;
    }
    if (observation == 7) {
        if (f.plane->vtbl->abort(f.plane, f.update) != SIXEL_OK) goto end;
        ok = memcmp(f.view.rgb, "\12\24\36\12\24\36\12\24\36", 9) == 0;
        goto end;
    }
    if (!resolved && indexed_fixture_accept(&f) != SIXEL_OK) goto end;
    sixel_retained_read_unref(f.read);
    f.read = NULL;
    identity = f.image->identity;
    if (observation == 11) {
        ++f.receipt->identity.sequence;
        status = indexed_fixture_commit(&f);
        if (status != SIXEL_LOGIC_ERROR ||
                f.plane->vtbl->acquire_read(f.plane, &f.read) != SIXEL_OK ||
                sixel_retained_read_get_view(f.read, &f.view) != SIXEL_OK)
            goto end;
        ok = f.view.sequence == before.sequence &&
            memcmp(f.view.rgb, "\12\24\36\12\24\36\12\24\36", 9) == 0;
        goto end;
    }
    status = resolved
        ? f.plane->vtbl->advance_resolved(f.plane, f.update, &f.stats)
        : indexed_fixture_commit(&f);
    if (status != SIXEL_OK) goto end;
    if (observation == 8) {
        ok = f.stats.worker_threads == 0 && f.stats.painted_pixels == 289;
        goto end;
    }
    if (observation == 9) {
        ok = f.stats.paint_size == 2 && f.stats.paint[0] == 1 &&
            f.stats.paint[1] == 0;
        goto end;
    }
    if (observation == 10) {
        ok = f.stats.paint == NULL && f.stats.paint_size == 0;
        goto end;
    }
    if (observation == 12) {
        ok = indexed_fixture_commit(&f) == SIXEL_LOGIC_ERROR;
        goto end;
    }
    if (observation == 17 && f.plane->vtbl->invalidate(f.plane,
            SIXEL_RETAINED_SEEK) != SIXEL_OK) goto end;
    if (observation == 14 && f.plane->vtbl->set_device(f.plane, NULL) !=
            SIXEL_OK) goto end;
    if (f.plane->vtbl->acquire_read(f.plane, &f.read) != SIXEL_OK ||
            sixel_retained_read_get_view(f.read, &f.view) != SIXEL_OK)
        goto end;
    if (observation == 13) {
        ok = f.view.stream == identity.stream &&
            f.view.epoch == identity.epoch &&
            f.view.sequence == identity.sequence;
    } else if (observation == 14) {
        ok = memcmp(f.view.rgb, "\12\24\36\377\0\0\12\24\36", 9) == 0 &&
            sixel_retained_read_get_gpu_view(f.read, device, &gpu) ==
                SIXEL_FEATURE_ERROR;
    } else if (observation == 17) {
        ok = f.view.sequence == 0 && f.view.epoch > identity.epoch &&
            memcmp(f.view.valid, "\0\0\0", 3) == 0;
    } else if (observation == 19) {
        ok = f.stats.refinement_needed == 1;
    } else if (observation == 22) {
        ok = f.stats.painted_pixels == 0 &&
            memcmp(f.view.rgb, "\12\24\36\12\24\36\12\24\36", 9) == 0;
    } else {
        /* Literal red/green wire definitions give exact expected colors.
         * This canvas walk checks one selected geometry, not a size sweep. */
        ok = f.stats.worker_threads == 0;
        for (y = 0; y < f.view.description.height; ++y) {
            for (x = 0; x < f.view.description.width; ++x) {
                pixel = (size_t)y * f.view.description.width + x;
                inside = x >= f.image->description.origin_x &&
                    x < f.image->description.origin_x + 3 &&
                    y >= f.image->description.origin_y &&
                    y < f.image->description.origin_y + 5;
                local = (size_t)(y - f.image->description.origin_y) * 3 +
                    x - f.image->description.origin_x;
                rgb = (unsigned char *)f.view.rgb + pixel * 3;
                if (!inside) {
                    ok &= memcmp(rgb, "\21\21\21", 3) == 0 &&
                        f.view.valid[pixel] == 0;
                } else {
                    ok &= rgb[0] == (local % 2 ? 0 : 255) &&
                        rgb[1] == (local % 2 ? 255 : 0) && rgb[2] == 0 &&
                        f.view.valid[pixel] == 1;
                }
            }
        }
    }
end:
    indexed_fixture_destroy(&f);
    if (device != NULL) device->vtbl->unref(device);
    return ok ? 0 : 1;
}

/* Compare the same prepared arithmetic with uploaded and resident history.
 * This isolates addressing and transfer avoidance, not lookup correctness. */
int
retained_gpu_lookup_observe(int observation)
{
    sixel_gpu_palette_request_t request;
    sixel_gpu_palette_test_hooks_t hooks;
    sixel_gpu_device_interface_t *device;
    sixel_gpu_buffer_interface_t *rgb;
    sixel_gpu_buffer_interface_t *valid_buffer;
    unsigned char source[35 * 3];
    unsigned char history[11 * 9 * 3];
    unsigned char valid[11 * 9];
    unsigned char palette[16 * 3];
    unsigned char skip[35];
    unsigned char expected_skip[35];
    sixel_index_t actual[35];
    sixel_index_t expected[35];
    void *memory;
    SIXELSTATUS status;
    int i;
    int c;
    int pos;
    int ok;

    device = NULL;
    rgb = valid_buffer = NULL;
    ok = 0;
    status = sixel_gpu_device_acquire(&device);
    if (status == SIXEL_FEATURE_ERROR) return 77;
    if (status != SIXEL_OK) goto end;
    memset(&request, 0, sizeof(request));
    memset(&hooks, 0, sizeof(hooks));
    memset(skip, 0, sizeof(skip));
    for (i = 0; i < 48; ++i) palette[i] = (unsigned char)(i * 71 + 17);
    for (i = 0; i < 99; ++i) {
        valid[i] = i % 3 != 0;
        for (c = 0; c < 3; ++c)
            history[i * 3 + c] = (unsigned char)(i * 19 + c * 53);
    }
    for (i = 0; i < 35; ++i) {
        pos = (4 + i / 7) * 11 + 3 + i % 7;
        for (c = 0; c < 3; ++c)
            source[i * 3 + c] = i % 2 ? history[pos * 3 + c]
                : palette[(i % 16) * 3 + c];
    }
    request.policy = SIXEL_GPU_POLICY_FORCE;
    request.test_hooks = &hooks;
    request.dest = expected;
    request.pixels = source;
    request.pixel_count = 35;
    request.width = 7;
    request.height = 5;
    request.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    request.palette = palette;
    request.palette_size = sizeof(palette);
    request.palette_depth = 3;
    request.ncolors = 16;
    request.lut_policy = SIXEL_LUT_POLICY_ORDERED;
    request.method_for_diffuse = SIXEL_DIFFUSE_NONE;
    request.method_for_scan = SIXEL_SCAN_RASTER;
    request.sixdelta_enabled = request.has_6delta_accumulation = 1;
    request.accumulation_pixels = history;
    request.accumulation_pixels_size = sizeof(history);
    request.accumulation_width = 11;
    request.accumulation_height = 9;
    request.accumulation_origin_x = 3;
    request.accumulation_origin_y = 4;
    request.accumulation_valid_mask = valid;
    request.accumulation_valid_mask_size = sizeof(valid);
    request.accumulation_keycolor = 31;
    request.sixdelta_threshold = 8;
    request.accumulation_result_mask = skip;
    request.accumulation_result_mask_size = sizeof(skip);
    if (sixel_gpu_palette_apply(&request) != SIXEL_OK || !hooks.published)
        goto end;
    memcpy(expected_skip, skip, sizeof(skip));
    if (device->vtbl->create_buffer(device, sizeof(history), &rgb) !=
            SIXEL_OK || rgb->vtbl->shared_memory(rgb, &memory) != SIXEL_OK)
        goto end;
    memcpy(memory, history, sizeof(history));
    if (device->vtbl->create_buffer(device, sizeof(valid), &valid_buffer) !=
            SIXEL_OK || valid_buffer->vtbl->shared_memory(valid_buffer,
                &memory) != SIXEL_OK) goto end;
    memcpy(memory, valid, sizeof(valid));
    memset(skip, 0, sizeof(skip));
    memset(&hooks, 0, sizeof(hooks));
    request.dest = actual;
    request.retained_device = device;
    request.retained_rgb = rgb;
    request.retained_valid = valid_buffer;
    if (sixel_gpu_palette_apply(&request) != SIXEL_OK || !hooks.entered ||
            !hooks.published) goto end;
    if (observation == 0) ok = memcmp(actual, expected, sizeof(actual)) == 0;
    else if (observation == 1)
        ok = memcmp(skip, expected_skip, sizeof(skip)) == 0;
    else if (observation == 2) ok = hooks.retained_rgb_upload_bytes == 0;
    else ok = hooks.retained_valid_upload_bytes == 0;
end:
    if (rgb != NULL) rgb->vtbl->unref(rgb);
    if (valid_buffer != NULL) valid_buffer->vtbl->unref(valid_buffer);
    if (device != NULL) device->vtbl->unref(device);
    return ok ? 0 : 1;
}

static int
retained_gpu_sequence_image(indexed_fixture_t *f, int second)
{
    sixel_indexed_frame_description_t desc;

    desc = f->image->description;
    sixel_retained_read_unref(f->read);
    f->read = NULL;
    sixel_retained_update_unref(f->update);
    f->update = NULL;
    sixel_stream_receipt_unref(f->receipt);
    f->receipt = NULL;
    sixel_indexed_frame_unref(f->image);
    f->image = NULL;
    desc.width = desc.encoded_width = 2;
    desc.height = desc.encoded_height = 1;
    desc.plane_width = 8;
    desc.plane_height = 3;
    desc.origin_x = second ? 4 : 1;
    desc.origin_y = second ? 2 : 1;
    if (sixel_indexed_frame_new(&f->image, &desc, f->allocator) != SIXEL_OK)
        return 0;
    memset(f->image->indexes, second ? 1 : 0, f->image->pixels);
    return 1;
}

int
retained_gpu_sequence_observe(int observation)
{
    indexed_fixture_t f;
    sixel_gpu_device_interface_t *device;
    sixel_retained_plane_description_t desc;
    unsigned char *rgb;
    unsigned char *valid;
    SIXELSTATUS status;
    int phase;
    int x;
    int y;
    int first;
    int second;
    int legacy;
    int ok;
    size_t p;

    device = NULL;
    memset(&f, 0, sizeof(f));
    ok = 0;
    status = sixel_gpu_device_acquire(&device);
    if (status == SIXEL_FEATURE_ERROR) return 77;
    if (status != SIXEL_OK || !indexed_fixture_init(&f)) goto end;
    memset(&desc, 0, sizeof(desc));
    desc.width = 8;
    desc.height = 3;
    desc.color_definition = SIXEL_PALETTE_OUTPUT_LEGACY;
    desc.colorspace = SIXEL_COLORSPACE_GAMMA;
    if (f.plane->vtbl->configure(f.plane, &desc) != SIXEL_OK ||
            sixel_retained_plane_legacy_map(f.plane, &rgb, &valid) != SIXEL_OK)
        goto end;
    memset(rgb, 17, 8 * 3 * 3);
    memset(valid, 0, 8 * 3);
    sixel_retained_plane_legacy_unmap(f.plane);
    if (f.plane->vtbl->set_device(f.plane, device) != SIXEL_OK) goto end;
    for (phase = 0; phase < 2; ++phase) {
        if (!retained_gpu_sequence_image(&f, phase) ||
                retained_gpu_fixture_begin(&f, 0, 0) != SIXEL_OK ||
                indexed_fixture_seal(&f) != SIXEL_OK) goto end;
        if (sixel_retained_plane_test_gpu_fault(f.plane,
                phase == 0 && observation == 2 ? 2 : 0) != SIXEL_OK) goto end;
        status = f.plane->vtbl->stage(f.plane, f.update);
        if (phase == 0 && observation == 2) {
            if (status != SIXEL_RUNTIME_ERROR) goto end;
        } else if (status != SIXEL_OK) goto end;
        if (phase == 0 && (observation == 1 || observation == 2)) {
            if (f.plane->vtbl->abort(f.plane, f.update) != SIXEL_OK) goto end;
        } else if (indexed_fixture_accept(&f) != SIXEL_OK ||
                indexed_fixture_commit(&f) != SIXEL_OK) goto end;
        if (phase == 0 && observation == 3) {
            if (sixel_retained_plane_legacy_map(f.plane, &rgb, &valid) !=
                    SIXEL_OK) goto end;
            memset(rgb, 79, 3);
            valid[0] = 1;
            sixel_retained_plane_legacy_unmap(f.plane);
        }
        if (phase == 0 && observation == 4 &&
                f.plane->vtbl->invalidate(f.plane, SIXEL_RETAINED_SEEK) !=
                    SIXEL_OK) goto end;
    }
    if (f.plane->vtbl->acquire_read(f.plane, &f.read) != SIXEL_OK ||
            sixel_retained_read_get_view(f.read, &f.view) != SIXEL_OK)
        goto end;
    ok = f.stats.worker_threads == 0;
    for (y = 0; y < 3; ++y) {
        for (x = 0; x < 8; ++x) {
            first = y == 1 && (x == 1 || x == 2) &&
                observation != 1 && observation != 2;
            second = y == 2 && (x == 4 || x == 5);
            legacy = observation == 3 && y == 0 && x == 0;
            p = (size_t)y * 8 + x;
            rgb = (unsigned char *)f.view.rgb + p * 3;
            ok &= rgb[0] == (first ? 255 : second ? 0 : legacy ? 79 : 17);
            ok &= rgb[1] == (first ? 0 : second ? 255 : legacy ? 79 : 17);
            ok &= rgb[2] == (first || second ? 0 : legacy ? 79 : 17);
            ok &= f.view.valid[p] ==
                (second || legacy || (first && observation != 4));
        }
    }
end:
    indexed_fixture_destroy(&f);
    if (device != NULL) device->vtbl->unref(device);
    return ok ? 0 : 1;
}

int
retained_gpu_observe(int observation)
{
    return retained_gpu_observe_impl(observation, 0);
}

int
retained_gpu_resolved_observe(int observation)
{
    return retained_gpu_observe_impl(observation, 1);
}
