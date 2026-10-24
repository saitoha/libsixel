/* SPDX-License-Identifier: MIT */
/* Check resolved reservations and phase ordering with fixed expectations. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/compat_stub.h"
#include "src/dither.h"
#include "src/encoder.h"
#include "src/planner.h"
#include "allocation_runtime_common.h"

#define ALLOCATION_WIDTH 64
#define ALLOCATION_HEIGHT 96

static int
allocation_runtime_discard(char *data, int size, void *priv)
{
    (void)data;
    (void)priv;
    return size;
}

static int
allocation_runtime_encode(allocation_runtime_expected_t const *expected,
                          int sparse)
{
    sixel_encoder_t *encoder;
    sixel_frame_t *frame;
    sixel_output_t *output;
    sixel_dither_t const *dither;
    SIXELSTATUS status;
    unsigned char pixels[ALLOCATION_WIDTH * ALLOCATION_HEIGHT * 3];
    char threads[40];
    int i;
    int ok;

    encoder = NULL;
    frame = NULL;
    output = NULL;
    ok = 0;
    for (i = 0; i < (int)sizeof(pixels); ++i) {
        /* The dense fixture keeps diffusion enabled after quantization;
         * the sparse fixture fits exactly and disables it at that stage. */
        pixels[i] = sparse ? (unsigned char)((i / 3 % 2) * 255)
            : (unsigned char)(i * 53 % 251);
    }
    /* Exercise full budgets on the small fixture, independent of balanced
     * CPU caps and preprocessing size thresholds. The policy resolver has
     * separate coverage for the balanced hardware cap. */
    (void)snprintf(threads, sizeof(threads), "greedy:B%d",
                   expected->threads);
    status = sixel_encoder_new(&encoder, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (SIXEL_FAILED(sixel_encoder_setopt(encoder,
            SIXEL_OPTFLAG_THREADING_POLICY, threads)) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'p', "16")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'G', "off")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'E', "fast")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'd',
                expected->diffusion != NULL ? expected->diffusion : "fs"))) {
        goto end;
    }
    if (SIXEL_FAILED(sixel_output_new(&output,
            allocation_runtime_discard, NULL, NULL)) ||
            SIXEL_FAILED(sixel_frame_new(&frame, NULL))) {
        goto end;
    }
    status = sixel_frame_init_borrowed(frame, pixels,
        ALLOCATION_WIDTH, ALLOCATION_HEIGHT, SIXEL_PIXELFORMAT_RGB888,
        NULL, -1);
    if (SIXEL_FAILED(status) ||
            SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame,
                output))) {
        goto end;
    }
    /* The diagnostic topic retains the completed application object even
     * when the automatic palette is not entered in the encoder cache. */
    dither = (sixel_dither_t const *)encoder->diagnostic_dither;
    if (dither == NULL ||
            dither->method_for_diffuse != expected->effective_method ||
            dither->pipeline_last_mode != expected->mode ||
            dither->pipeline_last_dither_threads != expected->dither ||
            dither->pipeline_last_encode_threads != expected->encode ||
            dither->pipeline_last_band_height != expected->band_height ||
            dither->pipeline_last_band_overlap != expected->overlap) {
        fprintf(stderr, "Resolved allocation differs at T=%s\n", threads);
        goto end;
    }
    ok = 1;
end:
    if (output != NULL) {
        sixel_output_unref(output);
    }
    if (frame != NULL) {
        sixel_frame_unref(frame);
    }
    if (encoder != NULL) {
        sixel_encoder_unref(encoder);
    }
    return ok;
}

static int
allocation_runtime_log(allocation_runtime_expected_t const *expected,
                       int count, char const *path)
{
    FILE *file;
    char line[2048];
    char const *field;
    allocation_runtime_expected_t const *current;
    unsigned int dither_started;
    unsigned int dither_finished;
    unsigned int encode_started;
    unsigned int encode_finished;
    unsigned int mask;
    int session;
    int apply_started;
    int apply_finished;
    int body_started;
    int body_finished;
    int active;
    int job;
    int ok;

    file = sixel_compat_fopen(path, "rb");
    if (file == NULL) {
        return 0;
    }
    session = 0;
    current = NULL;
    dither_started = dither_finished = 0u;
    encode_started = encode_finished = 0u;
    apply_started = apply_finished = body_started = body_finished = 0;
    active = 0;
    ok = 1;
    while (fgets(line, sizeof(line), file) != NULL) {
        if (strstr(line, "\"event\":\"encode_begin\"") != NULL) {
            if (current != NULL || session >= count) {
                ok = 0;
                break;
            }
            current = &expected[session];
            dither_started = dither_finished = 0u;
            encode_started = encode_finished = 0u;
            apply_started = apply_finished = body_started = body_finished = 0;
            active = 0;
        }
        if (current == NULL) {
            continue;
        }
        if (strstr(line, "\"worker\":\"dither\"") != NULL) {
            if (strstr(line, "\"event\":\"apply_start\"") != NULL) {
                ok &= ++apply_started == 1;
                /* D=1 runs on the caller and need not emit band markers. */
                active += current->dither == 1;
            } else if (strstr(line,
                    "\"event\":\"apply_finish\"") != NULL) {
                ok &= apply_started == 1 && ++apply_finished == 1;
                active -= current->dither == 1;
            } else if (strstr(line, "\"event\":\"start\"") != NULL ||
                    strstr(line, "\"event\":\"finish\"") != NULL) {
                field = strstr(line, "\"job\":");
                if (field == NULL || sscanf(field, "\"job\":%d", &job)
                        != 1 || job < 0 || job >= current->band_jobs) {
                    ok = 0;
                    break;
                }
                mask = 1u << job;
                ok &= apply_started == 1 && apply_finished == 0;
                if (strstr(line, "\"event\":\"start\"") != NULL) {
                    ok &= (dither_started & mask) == 0u;
                    dither_started |= mask;
                    active += current->dither > 1;
                } else {
                    ok &= (dither_started & mask) != 0u;
                    ok &= (dither_finished & mask) == 0u;
                    dither_finished |= mask;
                    active -= current->dither > 1;
                }
            }
        }
        if (strstr(line, "\"worker\":\"encode\"") != NULL) {
            if (strstr(line, "\"event\":\"body_start\"") != NULL) {
                ok &= ++body_started == 1;
                if (current->mode != SIXEL_DITHER_ENCODE_PIPELINE) {
                    ok &= apply_finished == 1;
                }
                active += current->threads == 1;
            } else if (strstr(line,
                    "\"event\":\"body_finish\"") != NULL) {
                ok &= body_started == 1 && ++body_finished == 1;
                active -= current->threads == 1;
            } else if (strstr(line,
                    "\"event\":\"worker_start\"") != NULL ||
                    strstr(line, "\"event\":\"worker_done\"") != NULL) {
                field = strstr(line, "\"job\":");
                if (field == NULL || sscanf(field, "\"job\":%d", &job)
                        != 1 || job < 0 || job >= ALLOCATION_HEIGHT / 6) {
                    ok = 0;
                    break;
                }
                mask = 1u << job;
                ok &= body_started == 1 && body_finished == 0;
                if (current->mode != SIXEL_DITHER_ENCODE_PIPELINE) {
                    ok &= apply_finished == 1;
                }
                if (strstr(line,
                        "\"event\":\"worker_start\"") != NULL) {
                    ok &= (encode_started & mask) == 0u;
                    encode_started |= mask;
                    ++active;
                } else {
                    ok &= (encode_started & mask) != 0u;
                    ok &= (encode_finished & mask) == 0u;
                    encode_finished |= mask;
                    --active;
                }
            }
        }
        /* This bounds occupied callbacks, including the serial caller.
         * It does not count the existing writer/controller as work slots. */
        ok &= active >= 0 && active <= current->threads;
        if (strstr(line, "\"event\":\"encode_done\"") != NULL) {
            ok &= active == 0 && apply_started == 1 && apply_finished == 1;
            ok &= body_started == 1 && body_finished == 1;
            ok &= dither_started == dither_finished;
            if (current->dither > 1) {
                ok &= dither_started == (1u << current->band_jobs) - 1u;
            }
            mask = current->threads == 1 ? 0u
                : (1u << (ALLOCATION_HEIGHT / 6)) - 1u;
            ok &= encode_started == mask && encode_finished == mask;
            ++session;
            current = NULL;
        }
        if (!ok) {
            fprintf(stderr, "Allocation phase session %d: %s", session,
                line);
            break;
        }
    }
    ok &= !ferror(file) && session == count && current == NULL;
    (void)fclose(file);
    return ok;
}

int
allocation_runtime_cases(allocation_runtime_expected_t const *expected,
                         int count, int sparse, char const *path)
{
    int i;
    int ok;

#if defined(__PCC__) || defined(__TINYC__)
    /* These compilers deliberately disable parallel palette application. */
    (void)expected;
    (void)count;
    (void)sparse;
    (void)path;
    fputs("allocation: compiler disables parallel apply\n", stderr);
    return EXIT_SUCCESS;
#endif
    (void)remove(path);
    if (sixel_compat_setenv("SIXEL_LOG_PATH", path) != 0 ||
            sixel_compat_setenv("SIXEL_TRACE_TOPIC", "dither_contract") != 0 ||
            sixel_compat_setenv("SIXEL_THREADING_DITHER", "") != 0 ||
            sixel_compat_setenv("SIXEL_DITHER_PARALLEL_BAND_WIDTH", "") != 0 ||
            sixel_compat_setenv("SIXEL_DITHER_PARALLEL_BAND_OVERWRAP", "")
                != 0) {
        return EXIT_FAILURE;
    }
    ok = 1;
    for (i = 0; i < count && ok; ++i) {
        ok = allocation_runtime_encode(&expected[i], sparse);
    }
    /* Completed encoders release their loggers and flush the shared writer,
     * including the close needed by native Windows/OpenVMS readers. */
    if (ok) {
        ok = allocation_runtime_log(expected, count, path);
    }
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
