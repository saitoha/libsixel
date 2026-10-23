/*
 * SPDX-License-Identifier: MIT
 *
 * Pin the timeline session that a direct sixel_encoder_encode_frame() call
 * records when SIXEL_LOG_PATH is set: each call is bracketed by its own
 * main/encoder begin and done events around the palette and band stages.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "src/compat_stub.h"

#define TIMELINE_0006_LOG_PATH "0006_timeline_encode_frame_session.jsonl"
#define TIMELINE_0006_WIDTH 16
#define TIMELINE_0006_HEIGHT 12
#define TIMELINE_0006_CALLS 2

static int
timeline_0006_discard(char *data, int size, void *priv)
{
    (void)data;
    (void)priv;
    return size;
}

static int
timeline_0006_encode_twice(void)
{
    SIXELSTATUS status;
    sixel_encoder_t *encoder;
    sixel_frame_t *frame;
    sixel_output_t *output;
    unsigned char pixels[TIMELINE_0006_WIDTH * TIMELINE_0006_HEIGHT * 3];
    int i;
    int call;
    int success;

    encoder = NULL;
    frame = NULL;
    output = NULL;
    success = 0;
    for (i = 0; i < TIMELINE_0006_WIDTH * TIMELINE_0006_HEIGHT * 3; i++) {
        pixels[i] = (unsigned char)(i * 37 % 251);
    }
    status = sixel_encoder_new(&encoder, NULL);
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_encoder_setopt(encoder,
                                      SIXEL_OPTFLAG_THREADING_POLICY, "1");
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_output_new(&output, timeline_0006_discard, NULL, NULL);
    }
    for (call = 0; SIXEL_SUCCEEDED(status) && call < TIMELINE_0006_CALLS;
         call++) {
        status = sixel_frame_new(&frame, NULL);
        if (SIXEL_SUCCEEDED(status)) {
            status = sixel_frame_init_borrowed(frame, pixels,
                                               TIMELINE_0006_WIDTH,
                                               TIMELINE_0006_HEIGHT,
                                               SIXEL_PIXELFORMAT_RGB888,
                                               NULL, -1);
        }
        if (SIXEL_SUCCEEDED(status)) {
            status = sixel_encoder_encode_frame(encoder, frame, output);
        }
        if (frame != NULL) {
            sixel_frame_unref(frame);
            frame = NULL;
        }
    }
    success = SIXEL_SUCCEEDED(status);
    if (!success) {
        fprintf(stderr, "encode_frame failed: %s\n",
                sixel_helper_get_additional_message());
    }
    if (output != NULL) {
        sixel_output_unref(output);
    }
    if (encoder != NULL) {
        sixel_encoder_unref(encoder);
    }
    return success;
}

static char *
timeline_0006_read_log(void)
{
    FILE *file;
    char *text;
    long size;
    size_t length;

    text = NULL;
    file = sixel_compat_fopen(TIMELINE_0006_LOG_PATH, "rb");
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

/* Each call must log begin, then palette work, then done, in order. */
static int
timeline_0006_sessions_are_bracketed(char const *text)
{
    static char const begin[] =
        "\"worker\":\"main\",\"role\":\"encoder\",\"event\":\"encode_begin\"";
    static char const done[] =
        "\"worker\":\"main\",\"role\":\"encoder\",\"event\":\"encode_done\"";
    static char const palette[] = "\"worker\":\"palette/build\"";
    char const *cursor;
    char const *start;
    char const *stage;
    char const *finish;
    int call;

    cursor = text;
    for (call = 0; call < TIMELINE_0006_CALLS; call++) {
        start = strstr(cursor, begin);
        if (start == NULL) {
            return 0;
        }
        stage = strstr(start, palette);
        finish = strstr(start, done);
        if (stage == NULL || finish == NULL || stage > finish) {
            return 0;
        }
        /* No second begin may open inside this session. */
        cursor = strstr(start + 1, begin);
        if (cursor != NULL && cursor < finish) {
            return 0;
        }
        cursor = finish + 1;
    }
    return strstr(cursor, begin) == NULL;
}

int
test_timeline_0006_timeline_encode_frame_session(int argc, char **argv)
{
    char *text;
    int success;

    (void)argc;
    (void)argv;
    text = NULL;
    success = 0;
    (void)remove(TIMELINE_0006_LOG_PATH);
    if (sixel_compat_setenv("SIXEL_LOG_PATH", TIMELINE_0006_LOG_PATH) != 0) {
        goto end;
    }
    if (!timeline_0006_encode_twice()) {
        goto end;
    }
    text = timeline_0006_read_log();
    if (text == NULL) {
        fprintf(stderr, "timeline log was not written\n");
        goto end;
    }
    if (!timeline_0006_sessions_are_bracketed(text)) {
        fprintf(stderr, "encode_frame timeline sessions changed\n");
        goto end;
    }
    success = 1;

end:
    free(text);
    (void)remove(TIMELINE_0006_LOG_PATH);
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
