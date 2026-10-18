/* SPDX-License-Identifier: MIT */
/* Scheduling consistency; independent numeric oracles live one level up. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/colorspace.h"
#include "src/compat_stub.h"
#include "src/threading.h"
#include "src/options.h"

static int const parallel_formats[] = {
    SIXEL_PIXELFORMAT_RGBFLOAT32, SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
    SIXEL_PIXELFORMAT_CIELABFLOAT32, SIXEL_PIXELFORMAT_OKLABFLOAT32,
    SIXEL_PIXELFORMAT_DIN99DFLOAT32
};
static int const parallel_spaces[] = {
    SIXEL_COLORSPACE_GAMMA, SIXEL_COLORSPACE_LINEAR,
    SIXEL_COLORSPACE_CIELAB, SIXEL_COLORSPACE_OKLAB,
    SIXEL_COLORSPACE_DIN99D
};

static SIXELSTATUS
parallel_convert(float *pixels, int src, int dst, int frame_api)
{
    sixel_frame_t *frame;
    SIXELSTATUS status;

    if (!frame_api) {
        return sixel_helper_convert_colorspace((unsigned char *)pixels,
            257U * 3U * sizeof(float), parallel_formats[src],
            parallel_spaces[src], parallel_spaces[dst]);
    }
    frame = NULL;
    status = sixel_frame_new(&frame, NULL);
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_frame_init_borrowed(frame, pixels, 257, 1,
            parallel_formats[src], NULL, -1);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_frame_set_pixelformat(frame, parallel_formats[dst]);
    }
    if (SIXEL_FAILED(status) && frame != NULL &&
            (sixel_frame_get_pixelformat(frame) != parallel_formats[src] ||
             sixel_frame_get_colorspace(frame) != parallel_spaces[src])) {
        status = SIXEL_LOGIC_ERROR;
    }
    sixel_frame_unref(frame);
    return status;
}

int
parallel_float_values(int frame_api)
{
    float input[257 * 3];
    float serial[257 * 3];
    float parallel[257 * 3];
    double linear[3];
    double typed[3];
    int src;
    int dst;
    int i;
    int c;
    int budget;

    for (src = 0; src < 5; ++src) {
        for (i = 0; i < 257; ++i) {
            /* Extended sRGB coordinates with positive luminance keep all
             * five forward/inverse domains valid, including DIN99d. */
            linear[0] = -.125 + (double)(i % 17) / 16.0;
            linear[1] = .5;
            linear[2] = 1.25;
            if (!sixel_color_convert(typed, linear, SIXEL_COLORSPACE_LINEAR,
                    parallel_spaces[src])) {
                return EXIT_FAILURE;
            }
            for (c = 0; c < 3; ++c) {
                input[i * 3 + c] = (float)typed[c];
            }
        }
        for (dst = 0; dst < 5; ++dst) {
            memcpy(serial, input, sizeof(input));
            sixel_set_threads(1);
            if (SIXEL_FAILED(parallel_convert(serial, src, dst, frame_api))) {
                return EXIT_FAILURE;
            }
            for (budget = 2; budget <= 8; budget *= 2) {
                memcpy(parallel, input, sizeof(input));
                sixel_set_threads(budget);
                if (SIXEL_FAILED(parallel_convert(parallel, src, dst,
                        frame_api)) ||
                        memcmp(serial, parallel, sizeof(serial)) != 0) {
                    fprintf(stderr, "parallel bits differ: %d -> %d\n",
                        src, dst);
                    return EXIT_FAILURE;
                }
            }
        }
    }
    return EXIT_SUCCESS;
}

int
parallel_float_atomic(int frame_api)
{
    float pixels[257 * 3];
    float saved[257 * 3];
    int i;
    SIXELSTATUS status;

    for (i = 0; i < 257 * 3; ++i) {
        pixels[i] = .25f;
    }
    pixels[257 * 3 - 1] = NAN;
    memcpy(saved, pixels, sizeof(saved));
    sixel_set_threads(4);
    status = parallel_convert(pixels, 1, 2, frame_api);
    if (status != SIXEL_BAD_INPUT || memcmp(saved, pixels, sizeof(saved)) ||
            strcmp(sixel_helper_get_additional_message(),
                "float frame conversion: nonfinite or domain error.") != 0) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

static int
parallel_sink(char *data, int size, void *context)
{
    (void)data;
    (void)context;
    return size;
}

/* Parse only logger-owned structural JSON fields, never elapsed times. */
static int
parallel_trace(char const *path, int clustering)
{
    FILE *file;
    char line[1024];
    char *field;
    unsigned long long thread;
    unsigned long long filter_thread;
    unsigned long long main_thread;
    unsigned long long completed_thread;
    unsigned long long workers[16];
    unsigned int session;
    unsigned int controller_session;
    int in_filter;
    int in_controller;
    int worker_count;
    int completed;
    int palette_seen;
    int worker_finishes;
    int i;

    file = sixel_compat_fopen(path, "rb");
    if (file == NULL) {
        perror("open timeline log");
        return 0;
    }
    in_filter = in_controller = worker_count = completed = 0;
    filter_thread = main_thread = completed_thread = 0;
    palette_seen = worker_finishes = 0;
    controller_session = 0;
    while (fgets(line, sizeof(line), file) != NULL) {
        field = strstr(line, "\"thread\":");
        if (field == NULL || sscanf(field, "\"thread\":%llu", &thread) != 1) {
            continue;
        }
        field = strstr(line, "\"session_id\":");
        if (field == NULL || sscanf(field, "\"session_id\":%u",
                &session) != 1) {
            continue;
        }
        if (strstr(line, "\"event\":\"encode_begin\"") != NULL) {
            main_thread = thread;
        }
        if (completed && thread == completed_thread &&
                strstr(line, "\"worker\":\"palette/build\"") != NULL) {
            palette_seen = 1;
        }
        if (strstr(line, "\"event\":\"colorspace-start\"") != NULL &&
                main_thread != 0 &&
                (clustering ? thread != main_thread : thread == main_thread)) {
            if (!in_filter) {
                in_filter = 1;
                filter_thread = thread;
                worker_count = worker_finishes = 0;
            }
        } else if (strstr(line, "\"event\":\"colorspace-finish\"") != NULL
                && thread == filter_thread) {
            if (in_filter && in_controller == 2 && worker_count == 2 &&
                    worker_finishes == 2) {
                completed = 1;
                completed_thread = thread;
            }
            in_filter = in_controller = 0;
        } else if (in_filter && strstr(line,
                "\"worker\":\"colorspace\"") != NULL) {
            if (strstr(line, "\"role\":\"controller\"") != NULL &&
                    thread == filter_thread) {
                if (strstr(line, "\"event\":\"start\"") != NULL) {
                    in_controller = 1;
                    controller_session = session;
                } else if (strstr(line, "\"event\":\"finish\"") != NULL &&
                        session == controller_session && in_controller == 1) {
                    in_controller = 2;
                }
            } else if (in_controller == 1 && session == controller_session
                    && strstr(line, "\"role\":\"worker\"") != NULL) {
                if (strstr(line, "\"event\":\"finish\"") != NULL) {
                    ++worker_finishes;
                    continue;
                }
                for (i = 0; i < worker_count; ++i) {
                    if (workers[i] == thread) {
                        break;
                    }
                }
                if (i == worker_count && worker_count < 16) {
                    workers[worker_count++] = thread;
                }
            }
        }
    }
    fclose(file);
    return completed && (!clustering || palette_seen);
}

int
parallel_float_pipeline(int clustering, char const *path)
{
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    float pixels[64 * 64 * 3];
    SIXELSTATUS status;
    int i;
    int ok;

    encoder = NULL;
    output = NULL;
    frame = NULL;
    ok = 0;
    for (i = 0; i < 64 * 64 * 3; ++i) {
        pixels[i] = (float)(i % 251) / 250.0f;
    }
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, parallel_sink, NULL,
                NULL)) || SIXEL_FAILED(sixel_frame_new(&frame, NULL))) {
        goto end;
    }
    if (SIXEL_FAILED(sixel_encoder_setopt(encoder, '=', "4")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'p', "16")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'Q', "heckbert")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '4',
                "adaptive-grid")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '5', "none")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'W',
                clustering ? "gamma" : "cielab")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'X',
                clustering ? "cielab" : "gamma")) ||
            SIXEL_FAILED(sixel_frame_init_borrowed(frame, pixels, 64, 64,
                SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, -1))) {
        goto end;
    }
    status = sixel_encoder_encode_frame(encoder, frame, output);
    ok = SIXEL_SUCCEEDED(status);
end:
    sixel_output_unref(output);
    sixel_frame_unref(frame);
    sixel_encoder_unref(encoder);
    return ok && parallel_trace(path, clustering) ? EXIT_SUCCESS : EXIT_FAILURE;
}
