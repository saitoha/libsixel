/* SPDX-License-Identifier: MIT */
/* Float monochrome lookup must consume coordinates, not their object bytes. */
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include <6cells.h>
#include "src/factory.h"
#include "src/encoder.h"
#include "src/dither.h"

static SIXELSTATUS
ml_create(sixel_allocator_t *allocator, char const *name,
          sixel_lookup_policy_interface_t **lookup)
{
    sixel_factory_t *factory;
    void *service;
    SIXELSTATUS status;

    service = NULL;
    status = sixel_components_getservice("services/factory", &service);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    factory = service;
    status = factory->vtbl->create(factory, name, allocator, (void **)lookup);
    factory->vtbl->unref(factory);
    return status;
}

int
test_mono_lookup_values(int mode)
{
    static float const samples[][3] = {
        { 0, 0, 0 }, { .25f, .25f, .25f }, { .49f, .49f, .49f },
        { .51f, .51f, .51f }, { .75f, .75f, .75f }, { 1, 1, 1 },
        { 1, 0, 0 }, { 0, 1, 1 }, { -.25f, -.25f, -.25f },
        { 1.25f, 1.25f, 1.25f }, { .5f, .5f, .5f }
    };
    static int const expected[] = { 0, 0, 0, 1, 1, 1, 0, 1, 0, 1, 0 };
    sixel_allocator_t *allocator;
    sixel_lookup_policy_interface_t *policy;
    sixel_lookup_policy_interface_t *other;
    sixel_lookup_policy_prepare_request_t request;
    unsigned char palette[6];
    float entries[8];
    float pixel[3];
    float large[3];
    unsigned char byte_pixel[3];
    char const *name;
    int invert;
    int stride;
    int i;
    int j;
    int got;
    int want;
    int ok;

    allocator = NULL;
    policy = NULL;
    other = NULL;
    invert = mode == 1;
    stride = mode == 2 ? 4 : 3;
    ok = 0;
    name = invert ? "lookup/mono-lightbg.float32" :
        mode == 6 ? "lookup/mono-darkbg.8bit" :
        "lookup/mono-darkbg.float32";
    memset(&request, 0, sizeof(request));
    memset(entries, 0, sizeof(entries));
    for (i = 0; i < 2; ++i) {
        for (j = 0; j < 3; ++j) {
            palette[i * 3 + j] = (unsigned char)((i ^ invert) * 255);
            entries[i * stride + j] = (float)(i ^ invert);
        }
    }
    if (stride == 4) {
        entries[3] = -1000;
        entries[7] = 1000;
    }
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, malloc, calloc,
            realloc, free)) || SIXEL_FAILED(ml_create(allocator, name,
            &policy))) {
        goto end;
    }
    request.palette = palette;
    request.palette_float = mode == 3 || mode == 6 ? NULL : entries;
    request.depth = 3;
    request.float_depth = stride * (int)sizeof(float);
    request.reqcolor = 2;
    request.pixelformat = mode == 6 ? SIXEL_PIXELFORMAT_RGB888 :
        SIXEL_PIXELFORMAT_RGBFLOAT32;
    request.allocator = allocator;
    if (SIXEL_FAILED(policy->vtbl->prepare(policy, &request))) {
        goto end;
    }
    if (mode == 6) {
        /* This repair leaves the legacy byte threshold unchanged. */
        memset(byte_pixel, 85, sizeof(byte_pixel));
        if (policy->vtbl->map_pixel(policy, byte_pixel) != 0) {
            goto end;
        }
        memset(byte_pixel, 86, sizeof(byte_pixel));
        ok = policy->vtbl->map_pixel(policy, byte_pixel) == 1;
        goto end;
    }
    for (i = 0; i < (int)(sizeof(samples) / sizeof(samples[0])); ++i) {
        for (j = 0; j < 3; ++j) {
            pixel[j] = samples[i][j] * (mode == 3 ? 255 : 1);
        }
        /* An exact midpoint keeps the first entry in either order. */
        want = i == 10 ? 0 : expected[i] ^ invert;
        got = policy->vtbl->map_pixel(policy,
            (unsigned char const *)(void const *)pixel);
        if (got != want) {
            fprintf(stderr, "mono mode=%d sample=%d got=%d want=%d\n",
                mode, i, got, want);
            goto end;
        }
    }
    if (mode == 4) {
        /* Reprepare one object with half-white. The other retains full-white.
         * The query .3 is above the new midpoint .25 but below the old .5. */
        if (SIXEL_FAILED(ml_create(allocator, name, &other)) ||
                SIXEL_FAILED(other->vtbl->prepare(other, &request))) {
            goto end;
        }
        for (j = 0; j < 3; ++j) {
            entries[stride + j] = .5f;
            pixel[j] = .3f;
        }
        if (SIXEL_FAILED(policy->vtbl->prepare(policy, &request)) ||
                policy->vtbl->map_pixel(policy,
                    (unsigned char const *)(void const *)pixel) != 1 ||
                other->vtbl->map_pixel(other,
                    (unsigned char const *)(void const *)pixel) != 0) {
            goto end;
        }
    }
    if (mode == 5) {
        /* The sum is positive despite binary32 intermediate overflow.
         * Negation changes its sign. No gamut clamp is involved. */
        large[0] = FLT_MAX;
        large[1] = FLT_MAX;
        large[2] = -FLT_MAX;
        if (policy->vtbl->map_pixel(policy,
                (unsigned char const *)(void const *)large) != 1) {
            goto end;
        }
        for (j = 0; j < 3; ++j) {
            large[j] = -large[j];
        }
        if (policy->vtbl->map_pixel(policy,
                (unsigned char const *)(void const *)large) != 0) {
            goto end;
        }
    }
    ok = 1;
end:
    if (policy != NULL) {
        policy->vtbl->unref(policy);
    }
    if (other != NULL) {
        other->vtbl->unref(other);
    }
    sixel_allocator_unref(allocator);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

typedef struct ml_output {
    char data[4096];
    size_t size;
} ml_output_t;

static int
ml_write(char *data, int size, void *context)
{
    ml_output_t *out;

    out = context;
    if (size < 0 || (size_t)size >= sizeof(out->data) - out->size) {
        return -1;
    }
    memcpy(out->data + out->size, data, (size_t)size);
    out->size += (size_t)size;
    out->data[out->size] = '\0';
    return size;
}

int
test_mono_lookup_encoder(int mode)
{
    static struct { int flag; char const *value; } const options[] = {
        { '=', "balanced:B1" }, { '~', "auto" }, { '.', "float32" },
        { 'W', "gamma" }, { 'd', "none" }, { '0', "none" },
        { '_', "nearest" }, { 'A', "keep" }, { 'e', NULL }
    };
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    sixel_dither_t *dither;
    ml_output_t captured;
    float source[6];
    float saved[6];
    char const *body;
    char const *expected;
    int i;
    int ok;

    encoder = NULL;
    output = NULL;
    frame = NULL;
    ok = 0;
    memset(&captured, 0, sizeof(captured));
    for (i = 0; i < 3; ++i) {
        source[i] = mode == 2 ? .2f : .25f;
        source[i + 3] = mode == 2 ? .45f : .75f;
    }
    memcpy(saved, source, sizeof(saved));
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, ml_write, &captured,
                NULL))) {
        goto end;
    }
    for (i = 0; i < (int)(sizeof(options) / sizeof(options[0])); ++i) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, options[i].flag,
                options[i].value))) {
            goto end;
        }
    }
    /* Active snap is a no-op for -e, but retains the legacy admission
     * boundary. This deliberately exercises the specialized float class. */
    if ((mode == 1 &&
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'i', NULL))) ||
            (mode == 2 &&
             SIXEL_FAILED(sixel_encoder_setopt(encoder, 'd', "fs"))) ||
            SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
            SIXEL_FAILED(sixel_frame_init_borrowed(frame, source, 2, 1,
                SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, -1)) ||
            SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame, output))) {
        goto end;
    }
    dither = (sixel_dither_t *)encoder->diagnostic_dither;
    body = strstr(captured.data, "\"1;1;2;1");
    expected = mode == 1 ? "@\033\\" : "?@\033\\";
    /* Entry zero is omitted. Trailing omitted pixels need no SIXEL bytes. */
    ok = dither != NULL && !dither->use_rgb101_palette &&
        strchr(captured.data, '#') == NULL && body != NULL &&
        strcmp(body + strlen("\"1;1;2;1"), expected) == 0 &&
        memcmp(saved, source, sizeof(saved)) == 0;
end:
    if (!ok) {
        fprintf(stderr, "legacy mono mode=%d output=%s\n", mode,
            captured.data);
    }
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
