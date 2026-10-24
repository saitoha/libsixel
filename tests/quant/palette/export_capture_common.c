/* SPDX-License-Identifier: MIT */
/* Export storage must depend on palette size, not image area. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder.h"
#include "src/dither.h"

static int export_track;
static size_t export_requested;

static void *
export_malloc(size_t size)
{
    if (export_track) {
        export_requested += size;
    }
    return malloc(size);
}

static void *
export_calloc(size_t count, size_t size)
{
    if (export_track) {
        export_requested += count * size;
    }
    return calloc(count, size);
}

static void *
export_realloc(void *ptr, size_t size)
{
    if (export_track) {
        export_requested += size;
    }
    return realloc(ptr, size);
}

typedef struct export_stream {
    char bytes[65536];
    size_t size;
} export_stream_t;

static int
export_write(char *data, int size, void *context)
{
    export_stream_t *stream;

    stream = context;
    if (size < 0 || (size_t)size > sizeof(stream->bytes) - stream->size) {
        return -1;
    }
    memcpy(stream->bytes + stream->size, data, (size_t)size);
    stream->size += (size_t)size;
    return size;
}

static int
export_options(sixel_encoder_t *encoder, int native, char const *path,
               int builtin)
{
    static struct { int flag; char const *value; } const options[] = {
        {'=', "greedy:B1"}, {'b', "gray2"}, {'~', "none"}, {'d', "none"},
        {'Q', "heckbert"},
        {'F', "none"}, {'_', "none"}, {'E', "size"},
        {'X', "gamma"}, {'W', "gamma"}, {'t', "rgb"}, {'7', NULL},
        {'g', NULL}, {'o', "/dev/null"}, {'L', "builtin!"}
    };
    size_t i;

    for (i = 0; i < sizeof(options) / sizeof(options[0]); ++i) {
        if (!builtin && options[i].flag == 'b') {
            continue;
        }
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder,
                options[i].flag, options[i].value))) {
            return 0;
        }
    }
    return SIXEL_SUCCEEDED(sixel_encoder_setopt(encoder, '.',
            native ? "float32" : "8bit")) &&
        SIXEL_SUCCEEDED(sixel_encoder_setopt(encoder, 'U',
            native ? "display-p3" : "gamma")) &&
        (path == NULL || SIXEL_SUCCEEDED(sixel_encoder_setopt(
            encoder, 'M', path)));
}

int
test_export_memory(int native, char const *filename)
{
    static unsigned char const expected[] = {
        0, 0, 0, 85, 85, 85, 170, 170, 170, 255, 255, 255
    };
    sixel_allocator_t *allocator;
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    sixel_dither_t *dither;
    unsigned char bytes[64 * 32 * 3];
    float pixels[64 * 32 * 3];
    export_stream_t streams[2];
    size_t requested[2];
    size_t budget;
    SIXELSTATUS status;
    int i;
    int pass;
    int ok;

    allocator = NULL;
    encoder = NULL;
    output = NULL;
    frame = NULL;
    ok = 0;
    memset(streams, 0, sizeof(streams));
    for (i = 0; i < 64 * 32 * 3; ++i) {
        bytes[i] = (unsigned char)((i / 3) % 4 * 85);
        pixels[i] = bytes[i] / 255.f;
    }
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, export_malloc,
            export_calloc, export_realloc, free))) {
        goto end;
    }
    for (pass = 0; pass < 2; ++pass) {
        if (SIXEL_FAILED(sixel_encoder_new(&encoder, allocator)) ||
                !export_options(encoder, native,
                    pass ? "pal:/dev/null" : NULL, 1) ||
                SIXEL_FAILED(sixel_output_new(&output, export_write,
                    &streams[pass], allocator)) ||
                SIXEL_FAILED(sixel_frame_new(&frame, allocator)) ||
                SIXEL_FAILED(sixel_frame_init_borrowed(frame,
                    native ? (void *)pixels : (void *)bytes, 64, 32,
                    native ? SIXEL_PIXELFORMAT_RGBFLOAT32 :
                        SIXEL_PIXELFORMAT_RGB888, NULL, -1))) {
            goto end;
        }
        /* Setup, fixture storage and diagnostics are outside the measured
         * interval. A single worker makes allocator observation serial. */
        export_requested = 0;
        export_track = 1;
        status = filename == NULL ?
            sixel_encoder_encode_frame(encoder, frame, output) :
            sixel_encoder_encode(encoder, filename);
        export_track = 0;
        requested[pass] = export_requested;
        dither = encoder->diagnostic_dither;
        if (SIXEL_FAILED(status) || dither == NULL ||
                dither->use_rgb101_palette != native ||
                (pass && (!encoder->capture_valid ||
                    encoder->capture_ncolors != 4 ||
                    memcmp(encoder->capture_palette, expected,
                        sizeof(expected)) != 0))) {
            fprintf(stderr, "export pass=%d status=%d native=%d "
                "observed=%d valid=%d colors=%d\n", pass, status, native,
                dither == NULL ? -1 : dither->use_rgb101_palette,
                encoder->capture_valid, encoder->capture_ncolors);
            goto end;
        }
        sixel_frame_unref(frame);
        frame = NULL;
        sixel_output_unref(output);
        output = NULL;
        sixel_encoder_unref(encoder);
        encoder = NULL;
    }
    /* Frame capture needs only twelve RGB bytes. A full writer may also
     * allocate a platform-specific path buffer: allow 1 KiB, well below
     * either 4096-byte image copy that the old writer created. */
    budget = filename == NULL ? sizeof(expected) : 1024;
    fprintf(stderr, "export allocation requests: %lu -> %lu, budget=%lu\n",
        (unsigned long)requested[0], (unsigned long)requested[1],
        (unsigned long)budget);
    ok = requested[1] >= requested[0] &&
        requested[1] - requested[0] <= budget &&
        streams[0].size == streams[1].size &&
        memcmp(streams[0].bytes, streams[1].bytes, streams[0].size) == 0;
end:
    export_track = 0;
    if (!ok) {
        fprintf(stderr, "export memory contract: %s\n",
            sixel_helper_get_additional_message());
    }
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    sixel_allocator_unref(allocator);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
test_export_reuse(char const *filename, int isolated)
{
    sixel_encoder_t *first;
    sixel_encoder_t *second;
    int ok;

    first = NULL;
    second = NULL;
    ok = 0;
    if (SIXEL_FAILED(sixel_encoder_new(&first, NULL)) ||
            !export_options(first, 1, "pal:-", isolated) ||
            (!isolated && SIXEL_FAILED(sixel_encoder_setopt(first,
                'p', "6"))) ||
            SIXEL_FAILED(sixel_encoder_encode(first, filename))) {
        goto end;
    }
    if (isolated) {
        if (SIXEL_FAILED(sixel_encoder_new(&second, NULL)) ||
                !export_options(second, 0, "pal:-", 1)) {
            goto end;
        }
    } else {
        second = first;
        sixel_encoder_ref(second);
    }
    if (SIXEL_FAILED(sixel_encoder_setopt(second,
            isolated ? 'b' : 'p', isolated ? "gray1" : "2")) ||
            SIXEL_FAILED(sixel_encoder_encode(second, filename)) ||
            (isolated && SIXEL_FAILED(sixel_encoder_encode(first,
                filename)))) {
        goto end;
    }
    ok = 1;
end:
    if (!ok) {
        fprintf(stderr, "export reuse: %s\n",
            sixel_helper_get_additional_message());
    }
    sixel_encoder_unref(second);
    sixel_encoder_unref(first);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
