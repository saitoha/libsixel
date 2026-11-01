/* SPDX-License-Identifier: MIT */
/* Small in-memory streams, independent color equations and exact timelines.
 * The profile generator supplies input bytes, never the expected RGB values. */
#include <math.h>
#include "tests/_support/loader/pixelformat_test_common.h"
#include "tests/_support/loader/builtin/loader_builtin_gif_test_common.h"
#include "tests/_support/loader/gif_icc_test_common.h"
#include "src/cms.h"
#include "src/fromgif.h"
#include "src/loader-common.h"

typedef struct gifc_probe {
    int count;
    int byte_output;
    int gamma_output;
    int srgb;
    int power;
    int start;
    int replay;
    int frames;
    int alpha;
    int explicit_bg;
    unsigned char expected[3][12];
} gifc_probe_t;

static double
gifc_linear(double value)
{
    return value <= 0.04045 ? value / 12.92 :
        pow((value + 0.055) / 1.055, 2.4);
}

static SIXELSTATUS
gifc_capture(sixel_frame_t *frame, void *data)
{
    /* inverse(M_sRGB) * M_P3, calculated independently from D65 primaries. */
    static double const matrix[3][3] = {
        {1.2249401762805598, -0.2249401762805598, 0.0},
        {-0.0420569547096882, 1.0420569547096882, 0.0},
        {-0.0196375545903344, -0.0786360455506319, 1.0982736001409663}
    };
    gifc_probe_t *probe;
    int format;
    int source_frame;
    int i;
    int c;
    int k;
    unsigned char const *pixels;
    float const *linear;
    double v[3];
    double expected;
    double actual;
    sixel_frame_transparency_t alpha;
    SIXELSTATUS status;

    probe = data;
    format = sixel_frame_get_pixelformat(frame);
    source_frame = probe->replay ? probe->count % 2 :
        probe->count + probe->start;
    if (source_frame >= probe->frames ||
        sixel_frame_get_width(frame) != 2 ||
        sixel_frame_get_height(frame) != 2 ||
        sixel_frame_get_frame_no(frame) !=
            (probe->replay ? probe->count % 2 : probe->count) ||
        sixel_frame_get_loop_no(frame) !=
            (probe->replay ? probe->count / 2 : 0) ||
        sixel_frame_get_delay(frame) != 7 + source_frame) {
        fprintf(stderr, "GIF ICC timeline/count mismatch at %d\n",
                probe->count);
        return SIXEL_BAD_INPUT;
    }
    ++probe->count;
    if (probe->byte_output) {
        if (format != SIXEL_PIXELFORMAT_PAL8) {
            fprintf(stderr, "GIF ICC byte palette lost: %x\n", format);
            return SIXEL_BAD_INPUT;
        }
        status = sixel_frame_set_pixelformat(frame, SIXEL_PIXELFORMAT_RGB888);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    } else if (format != (probe->gamma_output ? SIXEL_PIXELFORMAT_RGBFLOAT32 :
            SIXEL_PIXELFORMAT_LINEARRGBFLOAT32)) {
        fprintf(stderr, "GIF ICC float format mismatch: %x\n", format);
        return SIXEL_BAD_INPUT;
    }
    pixels = sixel_frame_get_pixels(frame);
    linear = (float const *)pixels;
    for (i = 0; i < 4; ++i) {
        for (c = 0; c < 3; ++c) {
            expected = probe->expected[source_frame][i * 3 + c] / 255.0;
            v[c] = probe->power ? expected * expected : gifc_linear(expected);
        }
        for (c = 0; c < 3; ++c) {
            if (probe->byte_output) {
                if (pixels[i * 3 + c] !=
                    probe->expected[source_frame][i * 3 + c]) {
                    return SIXEL_BAD_INPUT;
                }
                continue;
            }
            expected = v[c];
            if (!probe->srgb && !(probe->explicit_bg && i != 0)) {
                expected = 0.0;
                for (k = 0; k < 3; ++k) {
                    expected += matrix[c][k] * v[k];
                }
            }
            if (probe->gamma_output) {
                expected = copysign(fabs(expected) <= 0.0031308 ?
                    12.92 * fabs(expected) :
                    1.055 * pow(fabs(expected), 1.0 / 2.4) - 0.055, expected);
            }
            actual = linear[i * 3 + c];
            /* Serialized ICC colorants have s15Fixed16 rounding. */
            if (!isfinite(actual) || fabs(actual - expected) > 3e-4) {
                fprintf(stderr, "GIF ICC f=%d p=%d c=%d: %.9g != %.9g\n",
                        source_frame, i, c, actual, expected);
                return SIXEL_BAD_INPUT;
            }
        }
    }
    memset(&alpha, 0, sizeof(alpha));
    status = sixel_frame_as_interface(frame)->vtbl->get_transparency(
        sixel_frame_as_interface(frame), &alpha);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if (probe->alpha && (!alpha.alpha_zero_is_transparent ||
        alpha.transparent_mask_size != 4u ||
        memcmp(alpha.transparent_mask, "\1\0\0\0", 4u) != 0)) {
        fprintf(stderr, "GIF ICC transparent mask mismatch\n");
        return SIXEL_BAD_INPUT;
    }
    return SIXEL_OK;
}

static void
gifc_extension(edge_writer_t *writer, unsigned char const *profile,
                size_t length, size_t block_limit)
{
    size_t n;

    edge_put_bytes(writer, (unsigned char const *)"\x21\xff\x0bICCRGBG1012",
                   14u);
    while (length != 0u) {
        n = length > block_limit ? block_limit : length;
        edge_put_u8(writer, (unsigned int)n);
        edge_put_bytes(writer, profile, n);
        profile += n;
        length -= n;
    }
    edge_put_u8(writer, 0u);
}

int
gif_icc_test(int specimen, int engine)
{
    static unsigned char const palette[12] = {
        255u, 0u, 0u, 0u, 255u, 0u, 0u, 0u, 255u, 64u, 128u, 192u
    };
    static unsigned char const local[12] = {
        64u, 128u, 192u, 0u, 0u, 255u, 0u, 255u, 0u, 255u, 0u, 0u
    };
    static unsigned char const indices[4] = {0u, 1u, 2u, 3u};
    static double const xy[8] = {
        0.3127, 0.3290, 0.64, 0.33, 0.30, 0.60, 0.15, 0.06
    };
    static double const gamma[3] = {2.0, 2.0, 2.0};
    unsigned char bytes[8192];
    unsigned char initial[4] = {3u, 1u, 2u, 3u};
    unsigned char bg[3] = {51u, 102u, 153u};
    unsigned char *icc;
    unsigned char *extracted;
    size_t icc_size;
    size_t extracted_size;
    size_t extension;
    size_t control;
    int n;
    int i;
    int j;
    int value;
    int previous_engine;
    int previous_policy;
    int reject;
    int result;
    int engine_available;
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_color_def_t *definition;
    sixel_chunk_t *chunk;
    sixel_loader_component_t *component;
    loader_probe_callback_state_t callback;
    gifc_probe_t probe;
    edge_writer_t writer;

    previous_engine = sixel_cms_get_engine();
    sixel_cms_set_engine((sixel_cms_engine_t)engine);
    engine_available = (int)sixel_cms_get_engine() == engine;
    sixel_cms_set_engine((sixel_cms_engine_t)previous_engine);
    if (!engine_available) {
        return 77;
    }
    allocator = NULL;
    definition = NULL;
    component = NULL;
    chunk = NULL;
    icc = NULL;
    extracted = NULL;
    result = 1;
    previous_policy = loader_transparent_policy();
    sixel_helper_set_loader_transparent_policy(SIXEL_LOADER_ALPHA_POLICY_KEEP);
    memset(&probe, 0, sizeof(probe));
    probe.srgb = specimen == 3 || specimen == 4;
    probe.power = specimen == 4;
    probe.byte_output = specimen == 7 || specimen == 8 || specimen == 19 ||
        specimen == 26;
    probe.gamma_output = specimen == 29 || specimen == 37;
    probe.frames = specimen == 13 || specimen == 14 ? 3 :
        (specimen >= 16 && specimen <= 18) || specimen == 37 ? 2 : 1;
    probe.start = specimen == 16 || specimen == 17;
    probe.replay = specimen == 18 || specimen == 37;
    probe.alpha = specimen == 9;
    probe.explicit_bg = specimen == 11 || specimen == 36;
    reject = specimen >= 20 && specimen <= 25;
    for (i = 0; i < 3; ++i) {
        memcpy(probe.expected[i], palette, sizeof(palette));
    }
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    definition = probe.power
        ? sixel_cdef_rgb(xy, SIXEL_COLOR_TRC_POWER, gamma) :
        sixel_cdef_name(probe.srgb ? "srgb" : "display-p3");
    if (definition == NULL || SIXEL_FAILED(sixel_cdef_export_icc(
            definition, &icc, &icc_size, allocator))) {
        goto cleanup;
    }
    if (specimen == 20) {
        memset(icc, 0, icc_size);
    } else if (specimen == 21) {
        memcpy(icc + 16, "GRAY", 4u);
    }
    writer = (edge_writer_t){bytes, sizeof(bytes), 0u, 0};
    edge_gif_begin(&writer, 1, 2u, 2u);
    memcpy(bytes + 13, palette, sizeof(palette));
    if (specimen == 35 || specimen == 36) {
        bytes[10] = 0u;
        writer.length = 13u;
    }
    extension = writer.length;
    if (specimen != 6 && specimen != 7) {
        gifc_extension(&writer, icc, specimen == 23 ? 0u : icc_size,
                       specimen == 5 ? 1u : 255u);
        if (specimen == 19) {
            bytes[extension + 3u] = 'X';
        }
        if (specimen == 22 || specimen == 26) {
            gifc_extension(&writer, icc, icc_size, 255u);
        }
        if (specimen == 24) {
            writer.length -= 3u;
        }
    }
    if (probe.replay) {
        edge_put_bytes(&writer, (unsigned char const *)
            "\x21\xff\x0bNETSCAPE2.0\x03\x01\x02\x00\x00", 19u);
    }
    for (n = 0; n < probe.frames; ++n) {
        control = writer.length;
        edge_gif_graphic_control_delay(&writer,
            specimen == 13 && n == 1 ? 2u :
            specimen == 14 && n == 1 ? 3u : 1u, (unsigned int)n + 7u);
        if (probe.alpha) {
            bytes[control + 3u] |= 1u;
        }
        if ((specimen >= 10 && specimen <= 12) || specimen == 36) {
            if (specimen == 36) {
                edge_gif_local_image(&writer, 0u, 0u, 1u, 1u,
                                      palette, indices, 1u);
            } else {
                edge_gif_image(&writer, 0u, 0u, 1u, 1u, 0, indices, 1u, 1);
            }
            for (i = 1; i < 4; ++i) {
                memcpy(probe.expected[n] + i * 3,
                       probe.explicit_bg ? bg : palette, 3u);
            }
        } else if ((specimen == 13 || specimen == 14) && n != 0) {
            edge_gif_local_image(&writer, n == 1 ? 0u : 1u, 0u, 1u, 1u,
                                  local, n == 1 ? indices + 1 : indices, 1u);
            memcpy(probe.expected[n], palette, sizeof(palette));
            memcpy(probe.expected[n] + (n == 1 ? 0 : 3),
                   n == 1 ? local + 3 : local, 3u);
            if (n == 2 && specimen == 14) {
                memcpy(probe.expected[n], palette + 9, 3u);
            }
        } else if ((specimen == 13 || specimen == 14) && n == 0) {
            edge_gif_image(&writer, 0u, 0u, 2u, 2u, 0, initial, 4u, 1);
            memcpy(probe.expected[n], palette + 9, 3u);
        } else if (specimen == 2 || specimen == 35 ||
                   (probe.frames == 2 && n == 1)) {
            edge_gif_local_image(&writer, 0u, 0u, 2u, 2u, local, indices, 4u);
            memcpy(probe.expected[n], local, sizeof(local));
        } else {
            edge_gif_image(&writer, 0u, 0u, 2u, 2u,
                           specimen == 15, indices, 4u, 1);
        }
    }
    if (specimen == 6) {
        gifc_extension(&writer, icc, icc_size, 255u);
    }
    edge_put_u8(&writer, 0x3bu);
    if (specimen == 25) {
        --writer.length;
    }
    if (writer.failed) {
        goto cleanup;
    }
    if (specimen == 5 || specimen == 6) {
        status = sixel_gif_read_icc(bytes, (int)writer.length, &extracted,
                                   &extracted_size, allocator);
        if (status != SIXEL_OK || extracted_size != icc_size ||
            memcmp(extracted, icc, icc_size) != 0) {
            goto cleanup;
        }
    }
    status = sixel_chunk_create_from_memory(&chunk, bytes, writer.length, NULL,
                                             allocator);
    if (SIXEL_FAILED(status) || SIXEL_FAILED(create_loader_component_by_name(
            "builtin", allocator, (void **)&component))) {
        goto cleanup;
    }
    if (specimen == 8 || specimen == 26) {
        engine = SIXEL_CMS_ENGINE_NONE;
    }
    status = sixel_loader_component_setopt(component,
        SIXEL_LOADER_COMPONENT_OPTION_CMS_ENGINE, &engine);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    value = 1;
    status = sixel_loader_component_setopt(component,
        SIXEL_LOADER_OPTION_USE_PALETTE, &value);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    value = 0;
    status = sixel_loader_component_setopt(component,
        SIXEL_LOADER_COMPONENT_OPTION_PREFER_FLOAT32, &value);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    value = probe.frames == 1;
    status = sixel_loader_component_setopt(component,
        SIXEL_LOADER_OPTION_REQUIRE_STATIC, &value);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    value = probe.replay ? SIXEL_LOOP_AUTO : SIXEL_LOOP_DISABLE;
    status = sixel_loader_component_setopt(component,
        SIXEL_LOADER_OPTION_LOOP_CONTROL, &value);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    if (probe.start) {
        value = specimen == 16 ? 1 : -1;
        status = sixel_loader_component_setopt(component,
            SIXEL_LOADER_OPTION_START_FRAME_NO, &value);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
    }
    if (specimen == 11 || specimen == 12 || specimen == 36) {
        status = sixel_loader_component_setopt(component,
            SIXEL_LOADER_OPTION_BGCOLOR, bg);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
    }
    callback = (loader_probe_callback_state_t){NULL, gifc_capture, &probe};
    status = sixel_loader_component_load(component, chunk,
                                           capture_frame_trampoline, &callback);
    if (reject) {
        result = SIXEL_FAILED(status) && probe.count == 0 ? 0 : 1;
    } else {
        j = probe.replay ? 4 : probe.frames - probe.start;
        result = status == SIXEL_OK && probe.count == j ? 0 : 1;
    }
    if (result != 0) {
        fprintf(stderr, "GIF ICC specimen=%d status=%x frames=%d: %s\n",
                specimen, status, probe.count,
                sixel_helper_get_additional_message());
    }
cleanup:
    sixel_helper_set_loader_transparent_policy(previous_policy);
    sixel_loader_component_unref(component);
    if (chunk != NULL) {
        chunk->vtbl->unref(chunk);
    }
    sixel_cdef_free(definition);
    if (allocator != NULL) {
        sixel_allocator_free(allocator, extracted);
        sixel_allocator_free(allocator, icc);
        sixel_allocator_unref(allocator);
    }
    return result;
}

/* Failure injection belongs at the format-independent palette hook, not in
 * the CMS implementation. A partially written failed palette is never drawn. */
typedef struct gifc_fault {
    int transforms;
    int callbacks;
    int fail_after;
} gifc_fault_t;

static SIXELSTATUS
gifc_fail_palette(void *data, unsigned char const *rgb, float *out, int count)
{
    gifc_fault_t *fault;
    int i;

    fault = data;
    ++fault->transforms;
    out[0] = 123.0f;
    if (fault->transforms > fault->fail_after) {
        return SIXEL_BAD_INPUT;
    }
    for (i = 0; i < count * 3; ++i) {
        out[i] = (float)rgb[i] / 255.0f;
    }
    return SIXEL_OK;
}

static SIXELSTATUS
gifc_count_frame(sixel_frame_t *frame, void *data)
{
    gifc_fault_t *fault;

    fault = data;
    if (sixel_frame_get_pixelformat(frame) !=
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32) {
        return SIXEL_BAD_INPUT;
    }
    ++fault->callbacks;
    return SIXEL_OK;
}

static void *
gifc_fail_canvas(size_t size)
{
    /* Two by two RGB float: fail the canvas allocation, not setup objects. */
    return size == 4u * 3u * sizeof(float) ? NULL : malloc(size);
}

int
gif_icc_failure_test(int specimen)
{
    unsigned char bytes[128];
    unsigned char index[4] = {0u, 1u, 2u, 3u};
    edge_writer_t writer;
    sixel_allocator_t *allocator;
    sixel_gif_color_t color;
    gifc_fault_t fault;
    SIXELSTATUS status;
    union {
        sixel_load_image_function function;
        void *pointer;
    } callback;
    int result;

    allocator = NULL;
    status = sixel_allocator_new(&allocator,
        specimen == 32 ? gifc_fail_canvas : NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        return 1;
    }
    writer = (edge_writer_t){bytes, sizeof(bytes), 0u, 0};
    edge_gif_begin(&writer, 1, 2u, 2u);
    edge_gif_image(&writer, 0u, 0u, 2u, 2u, 0, index, 4u, 1);
    edge_gif_local_image(&writer, 0u, 0u, 2u, 2u,
                         edge_palette_rgb, index, 4u);
    edge_put_u8(&writer, 0x3bu);
    fault = (gifc_fault_t){0, 0, specimen == 30 ? 0 : 2};
    color = (sixel_gif_color_t){gifc_fail_palette, &fault, {0.0f, 0.0f, 0.0f}};
    callback.function = gifc_count_frame;
    status = load_gif_with_color(bytes, (int)writer.length, NULL, 0, 256, 1,
        0, SIXEL_LOOP_DISABLE, 0, callback.pointer, &fault, NULL, allocator,
        &color);
    result = !writer.failed && SIXEL_FAILED(status) &&
        fault.callbacks == (specimen == 31 ? 1 : 0) ? 0 : 1;
    if (result) {
        fprintf(stderr, "GIF fault %d status=%x calls=%d frames=%d\n",
                specimen, status, fault.transforms, fault.callbacks);
    }
    sixel_allocator_unref(allocator);
    return result;
}

int
gif_icc_limit_test(int over_limit)
{
    unsigned char *bytes;
    unsigned char *profile;
    size_t size;
    size_t payload;
    size_t offset;
    size_t n;
    size_t profile_size;
    size_t i;
    sixel_allocator_t *allocator;
    SIXELSTATUS status;
    int result;

    allocator = NULL;
    profile = NULL;
    payload = SIXEL_GIF_ICC_MAX_BYTES + (over_limit ? 1u : 0u);
    size = payload + (payload + 254u) / 255u + 29u;
    bytes = malloc(size);
    if (bytes == NULL || SIXEL_FAILED(sixel_allocator_new(&allocator,
            NULL, NULL, NULL, NULL))) {
        free(bytes);
        return 1;
    }
    memcpy(bytes, "GIF89a\x01\0\x01\0\0\0\0", 13u);
    memcpy(bytes + 13u, "\x21\xff\x0bICCRGBG1012", 14u);
    offset = 27u;
    i = 0u;
    while (i < payload) {
        n = payload - i > 255u ? 255u : payload - i;
        bytes[offset++] = (unsigned char)n;
        memset(bytes + offset, 0x5au, n);
        offset += n;
        i += n;
    }
    bytes[offset++] = 0u;
    bytes[offset++] = 0x3bu;
    status = sixel_gif_read_icc(bytes, (int)offset, &profile,
                               &profile_size, allocator);
    result = over_limit ? (SIXEL_FAILED(status) && profile == NULL ? 0 : 1) :
        (status == SIXEL_OK && profile_size == payload ? 0 : 1);
    if (!over_limit && !result) {
        for (i = 0u; i < profile_size; ++i) {
            if (profile[i] != 0x5au) {
                result = 1;
                break;
            }
        }
    }
    sixel_allocator_free(allocator, profile);
    sixel_allocator_unref(allocator);
    free(bytes);
    return result;
}
