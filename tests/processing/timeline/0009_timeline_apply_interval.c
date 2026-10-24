/*
 * SPDX-License-Identifier: MIT
 *
 * Palette application has one common interval for serial and band workers.
 * The shell wrapper owns the diagnostics policy coverage link.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "src/compat_stub.h"

#define TIMELINE_0009_LOG_PATH "0009_timeline_apply_interval.jsonl"
#define TIMELINE_0009_WIDTH 64
#define TIMELINE_0009_HEIGHT 64

static int
timeline_0009_discard(char *data, int size, void *priv)
{
    (void)data;
    (void)priv;
    return size;
}

static int
timeline_0009_encode(char const *threads)
{
    char policy[64];
    SIXELSTATUS status;
    sixel_encoder_t *encoder;
    sixel_frame_t *frame;
    sixel_output_t *output;
    unsigned char pixels[TIMELINE_0009_WIDTH * TIMELINE_0009_HEIGHT * 3];
    int i;

    encoder = NULL;
    frame = NULL;
    output = NULL;
    for (i = 0; i < TIMELINE_0009_WIDTH * TIMELINE_0009_HEIGHT * 3; i++) {
        pixels[i] = (unsigned char)(i * 53 % 251);
    }
    (void)snprintf(policy, sizeof(policy), "balanced:B%s", threads);
    status = sixel_encoder_new(&encoder, NULL);
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_encoder_setopt(encoder,
                                      SIXEL_OPTFLAG_THREADING_POLICY, policy);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_encoder_setopt(encoder, 'p', "16");
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_encoder_setopt(encoder, 'G', "off");
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_encoder_setopt(encoder, 'd', "fs");
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_output_new(&output, timeline_0009_discard, NULL, NULL);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_frame_new(&frame, NULL);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_frame_init_borrowed(frame, pixels,
                                           TIMELINE_0009_WIDTH,
                                           TIMELINE_0009_HEIGHT,
                                           SIXEL_PIXELFORMAT_RGB888,
                                           NULL, -1);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_encoder_encode_frame(encoder, frame, output);
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "encode with %s failed: %s\n", threads,
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
    return SIXEL_SUCCEEDED(status);
}

static char *
timeline_0009_read_log(void)
{
    FILE *file;
    char *text;
    long size;
    size_t length;

    text = NULL;
    file = sixel_compat_fopen(TIMELINE_0009_LOG_PATH, "rb");
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
timeline_0009_thread_of(char const *text, char const *position,
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

/* Check one encode session that starts at or after cursor. */
static char const *
timeline_0009_session(char const *text, char const *cursor)
{
    static char const begin_event[] = "\"event\":\"encode_begin\"";
    static char const done_event[] = "\"event\":\"encode_done\"";
    static char const apply_start[] = "\"event\":\"apply_start\"";
    static char const apply_finish[] = "\"event\":\"apply_finish\"";
    char const *begin;
    char const *done;
    char const *start;
    char const *finish;
    char const *next;
    char const *body_start;
    char const *body_finish;
    unsigned long long begin_thread;
    unsigned long long start_thread;
    unsigned long long finish_thread;

    begin = strstr(cursor, begin_event);
    done = begin != NULL ? strstr(begin, done_event) : NULL;
    start = begin != NULL ? strstr(begin, apply_start) : NULL;
    finish = start != NULL ? strstr(start, apply_finish) : NULL;
    if (done == NULL || start == NULL || finish == NULL || finish > done) {
        return NULL;
    }
    body_start = strstr(begin, "\"event\":\"body_start\"");
    body_finish = strstr(begin, "\"event\":\"body_finish\"");
    if (body_start == NULL || body_finish == NULL ||
            body_start > body_finish || body_finish > done) {
        return NULL;
    }
    next = strstr(body_start + 1, "\"event\":\"body_start\"");
    if (next != NULL && next < done) {
        return NULL;
    }
    next = strstr(body_finish + 1, "\"event\":\"body_finish\"");
    if (next != NULL && next < done) {
        return NULL;
    }
    /* Exactly one application interval belongs to this encode session. */
    next = strstr(start + 1, apply_start);
    if (next != NULL && next < done) {
        return NULL;
    }
    next = strstr(finish + 1, apply_finish);
    if (next != NULL && next < done) {
        return NULL;
    }
    if (!timeline_0009_thread_of(text, begin, &begin_thread) ||
            !timeline_0009_thread_of(text, start, &start_thread) ||
            !timeline_0009_thread_of(text, finish, &finish_thread) ||
            start_thread != begin_thread || finish_thread != begin_thread) {
        return NULL;
    }
    return done + 1;
}

int
test_timeline_0009_timeline_apply_interval(int argc, char **argv)
{
    char *text;
    char const *cursor;
    int success;

    (void)argc;
    (void)argv;
    text = NULL;
    success = 0;
    (void)remove(TIMELINE_0009_LOG_PATH);
    if (sixel_compat_setenv("SIXEL_LOG_PATH", TIMELINE_0009_LOG_PATH) != 0) {
        goto end;
    }
    if (!timeline_0009_encode("1") || !timeline_0009_encode("2") ||
            !timeline_0009_encode("4")) {
        goto end;
    }
    text = timeline_0009_read_log();
    if (text == NULL) {
        fprintf(stderr, "timeline log was not written\n");
        goto end;
    }
    cursor = timeline_0009_session(text, text);
    if (cursor != NULL) {
        cursor = timeline_0009_session(text, cursor);
    }
    if (cursor == NULL || timeline_0009_session(text, cursor) == NULL) {
        fprintf(stderr, "palette application interval contract changed\n");
        goto end;
    }
    success = 1;

end:
    free(text);
    (void)remove(TIMELINE_0009_LOG_PATH);
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
