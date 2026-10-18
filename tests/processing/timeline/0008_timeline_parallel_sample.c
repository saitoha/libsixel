/*
 * SPDX-License-Identifier: MIT
 *
 * Verify parallel sampling through the public encode-frame entry point.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "src/compat_stub.h"

#define TIMELINE_0008_LOG_PATH "0008_timeline_parallel_sample.jsonl"
#define TIMELINE_0008_WIDTH 641
#define TIMELINE_0008_HEIGHT 481

static int
timeline_0008_discard(char *data, int size, void *priv)
{
    (void)data;
    (void)priv;
    return size;
}

static int
timeline_0008_encode(char const *policy)
{
    SIXELSTATUS status;
    sixel_encoder_t *encoder;
    sixel_frame_t *frame;
    sixel_output_t *output;
    unsigned char *pixels;
    int i;

    encoder = NULL;
    frame = NULL;
    output = NULL;
    pixels = (unsigned char *)malloc(
        TIMELINE_0008_WIDTH * TIMELINE_0008_HEIGHT * 3u);
    if (pixels == NULL) {
        return 0;
    }
    for (i = 0; i < TIMELINE_0008_WIDTH * TIMELINE_0008_HEIGHT * 3; i++) {
        pixels[i] = (unsigned char)(i * 53 % 251);
    }
    status = sixel_encoder_new(&encoder, NULL);
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_THREADS, "4");
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_encoder_setopt(encoder, '4', policy);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_output_new(&output, timeline_0008_discard, NULL, NULL);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_frame_new(&frame, NULL);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_frame_init_borrowed(frame, pixels,
                                           TIMELINE_0008_WIDTH,
                                           TIMELINE_0008_HEIGHT,
                                           SIXEL_PIXELFORMAT_RGB888,
                                           NULL, -1);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_encoder_encode_frame(encoder, frame, output);
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "encode with %s failed: %s\n", policy,
                sixel_helper_get_additional_message());
    }
    if (frame != NULL) {
        sixel_frame_unref(frame);
    }
    if (output != NULL) {
        sixel_output_unref(output);
    }
    if (encoder != NULL) {
        sixel_encoder_unref(encoder);
    }
    free(pixels);
    return SIXEL_SUCCEEDED(status);
}

static char *
timeline_0008_read_log(void)
{
    FILE *file;
    char *text;
    long size;
    size_t length;

    text = NULL;
    file = sixel_compat_fopen(TIMELINE_0008_LOG_PATH, "rb");
    if (file == NULL) {
        perror("open timeline log");
        return NULL;
    }
    if (fseek(file, 0L, SEEK_END) == 0) {
        size = ftell(file);
        if (size > 0 && fseek(file, 0L, SEEK_SET) == 0) {
            text = (char *)malloc((size_t)size + 1u);
            if (text != NULL) {
                length = fread(text, 1u, (size_t)size, file);
                text[length] = '\0';
            }
        }
    }
    (void)fclose(file);
    return text;
}

/* Return the "thread" value of the JSON line containing position. */
static int
timeline_0008_thread_of(char const *text, char const *position,
                        unsigned long long *thread)
{
    char const *line;
    char const *field;

    line = position;
    while (line > text && line[-1] != '\n') {
        --line;
    }
    field = strstr(line, "\"thread\":");
    return field != NULL && field < strchr(position, '\n') &&
        sscanf(field, "\"thread\":%llu", thread) == 1;
}

/* Require nested intervals and two real thread identities, never elapsed
 * time or a particular scheduling order among independent workers. */
static int
timeline_0008_session(char const *text)
{
    char const *start;
    char const *finish;
    char const *line;
    char const *next;
    unsigned long long thread;
    unsigned long long first_thread;
    int distinct;
    int active;
    int controllers;
    int workers;

    start = strstr(text, "\"event\":\"sample-start\"");
    finish = strstr(text, "\"event\":\"sample-finish\"");
    if (start == NULL || finish == NULL || start >= finish) {
        return 0;
    }
    first_thread = 0;
    distinct = 0;
    active = 0;
    controllers = 0;
    workers = 0;
    line = strchr(start, '\n');
    while (line != NULL && ++line < finish) {
        next = strchr(line, '\n');
        start = strstr(line, "\"worker\":\"sample\"");
        if (next == NULL) {
            return 0;
        }
        if (start != NULL && start < next) {
            start = strstr(line, "\"role\":\"controller\"");
            if (start != NULL && start < next) {
                start = strstr(line, "\"event\":\"start\"");
                if (start != NULL && start < next) {
                    if (active) {
                        return 0;
                    }
                    active = 1;
                    ++controllers;
                } else {
                    if (!active || workers != 0) {
                        return 0;
                    }
                    active = 0;
                }
            } else {
                if (!active || !timeline_0008_thread_of(text, line, &thread)) {
                    return 0;
                }
                if (first_thread == 0) {
                    first_thread = thread;
                }
                distinct |= thread != first_thread;
                start = strstr(line, "\"event\":\"start\"");
                workers += start != NULL && start < next ? 1 : -1;
                if (workers < 0) {
                    return 0;
                }
            }
        }
        line = next;
    }
    return !active && controllers >= 2 && distinct;
}

int
test_timeline_0008_timeline_parallel_sample(int argc, char **argv)
{
    char *text;
    int success;

    (void)argc;
    (void)argv;
    text = NULL;
    success = 0;
#if !SIXEL_ENABLE_THREADS
    return EXIT_SUCCESS;
#endif
    (void)sixel_compat_setenv("SIXEL_SAMPLE_PARALLEL_MIN_PIXELS", "1");
    (void)remove(TIMELINE_0008_LOG_PATH);
    if (sixel_compat_setenv("SIXEL_LOG_PATH", TIMELINE_0008_LOG_PATH) != 0) {
        goto end;
    }
    if (!timeline_0008_encode("auto")) {
        goto end;
    }
    text = timeline_0008_read_log();
    if (text == NULL) {
        fprintf(stderr, "timeline log was not written\n");
        goto end;
    }
    if (!timeline_0008_session(text)) {
        fprintf(stderr, "sample interval contract changed\n");
        goto end;
    }
    success = 1;

end:
    free(text);
    (void)remove(TIMELINE_0008_LOG_PATH);
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
