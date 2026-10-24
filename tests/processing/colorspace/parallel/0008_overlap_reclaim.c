/* SPDX-License-Identifier: MIT */
/* A cheap solid palette precedes a large -W conversion. Assert donation,
 * pool expansion and subsequent work, not the scheduler's worker selection. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/compat_stub.h"

static int
reclaim_sink(char *data, int size, void *context)
{
    (void)data;
    (void)context;
    return size;
}

static int
reclaim_trace(char const *path)
{
    FILE *file;
    char line[1024];
    char *field;
    unsigned long long thread;
    unsigned long long main_thread;
    unsigned int session;
    unsigned int conversion_session;
    int palette_done;
    int returned;
    int expanded;
    int in_filter;
    int in_conversion;
    int count;
    int ok;

    file = sixel_compat_fopen(path, "rb");
    if (file == NULL) {
        perror("open timeline log");
        return 0;
    }
    main_thread = 0;
    conversion_session = 0;
    palette_done = returned = expanded = 0;
    in_filter = in_conversion = count = ok = 0;
    while (fgets(line, sizeof(line), file) != NULL) {
        field = strstr(line, "\"thread\":");
        if (field == NULL || sscanf(field, "\"thread\":%llu", &thread) != 1) {
            continue;
        }
        field = strstr(line, "\"session_id\":");
        if (field == NULL ||
                sscanf(field, "\"session_id\":%u", &session) != 1) {
            continue;
        }
        if (strstr(line, "\"event\":\"encode_begin\"") != NULL) {
            main_thread = thread;
            palette_done = returned = expanded = count = 0;
        }
        if (strstr(line, "\"worker\":\"palette/build\"") != NULL &&
                strstr(line, "\"role\":\"palette\"") != NULL &&
                strstr(line, "\"event\":\"finish\"") != NULL) {
            palette_done = 1;
        }
        if (thread == main_thread &&
                strstr(line, "\"event\":\"colorspace-start\"") != NULL) {
            in_filter = 1;
        }
        if (in_filter && in_conversion && thread == main_thread &&
                strstr(line, "\"event\":\"budget-return\"") != NULL) {
            returned = palette_done;
        }
        if (in_filter && strstr(line, "\"worker\":\"colorspace\"") != NULL) {
            if (thread == main_thread &&
                    strstr(line, "\"role\":\"controller\"") != NULL) {
                if (strstr(line, "\"event\":\"start\"") != NULL) {
                    conversion_session = session;
                    in_conversion = 1;
                } else if (in_conversion && returned &&
                        session == conversion_session &&
                        strstr(line, "\"event\":\"grow\"") != NULL) {
                    expanded = 1;
                } else if (strstr(line, "\"event\":\"finish\"") != NULL) {
                    in_conversion = 0;
                }
            } else if (in_conversion && expanded &&
                    session == conversion_session &&
                    strstr(line, "\"event\":\"finish\"") != NULL) {
                /* Existing workers may drain all queued chunks before a
                 * newly created thread is scheduled. Growth does not promise
                 * that every available worker receives a chunk. */
                ++count;
            }
        }
        if (thread == main_thread &&
                strstr(line, "\"event\":\"colorspace-finish\"") != NULL) {
            ok |= returned && expanded && count > 0;
            in_filter = 0;
        }
    }
    fclose(file);
    if (!ok) {
        fprintf(stderr, "palette reclaim: palette_done=%d returned=%d "
                "expanded=%d completed_chunks=%d conversion_session=%u\n",
                palette_done, returned, expanded, count, conversion_session);
    }
    return ok;
}

int
test_parallel_float_0008(int argc, char **argv)
{
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    unsigned char *pixels;
    SIXELSTATUS status;
    int ok;

    if (argc != 2) {
        return EXIT_FAILURE;
    }
    encoder = NULL;
    output = NULL;
    frame = NULL;
    ok = 0;
    pixels = malloc(1920U * 1080U * 3U);
    if (pixels == NULL) {
        return EXIT_FAILURE;
    }
    memset(pixels, 127, 1920U * 1080U * 3U);
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, reclaim_sink, NULL, NULL)) ||
            SIXEL_FAILED(sixel_frame_new(&frame, NULL))) {
        goto end;
    }
    if (SIXEL_FAILED(sixel_encoder_setopt(encoder, '=', "greedy:B8")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'Q', "kmeans")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'F', "ward")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'W', "cielab")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'X', "cielab")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'd', "lso2")) ||
            SIXEL_FAILED(sixel_frame_init_borrowed(frame, pixels, 1920, 1080,
                SIXEL_PIXELFORMAT_RGB888, NULL, -1))) {
        goto end;
    }
    status = sixel_encoder_encode_frame(encoder, frame, output);
    ok = SIXEL_SUCCEEDED(status);
end:
    sixel_output_unref(output);
    sixel_frame_unref(frame);
    sixel_encoder_unref(encoder);
    free(pixels);
    return ok && reclaim_trace(argv[1]) ? EXIT_SUCCESS : EXIT_FAILURE;
}
