/* SPDX-License-Identifier: MIT */
/* Lookup failures retain their status and diagnostic on every palette path. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "status.h"

static int
lookup_failure_write(char *data, int size, void *priv)
{
    (void)data;
    *(int *)priv += size;
    return size;
}

/* Exercise inline and heap-backed raw diagnostics, including markup escapes.
 * Taking/restoring must neither allocate nor render a borrowed TLS pointer. */
static int
lookup_failure_message(void)
{
    sixel_status_message_t message;
    char text[8192];
    char expected[8192];
    int i;

    memset(&message, 0, sizeof(message));
    for (i = 0; i < 2; i++) {
        memset(text, 'x', sizeof(text));
        memcpy(text, "\\fElookup\\fP \\\\ ", 16u);
        text[i == 0 ? 32u : sizeof(text) - 1u] = '\0';
        sixel_helper_set_additional_message(text);
        snprintf(expected, sizeof(expected), "%s",
                 sixel_helper_get_additional_message());
        sixel_status_take_message(&message);
        sixel_helper_set_additional_message("caller diagnostic");
        sixel_status_restore_message(&message);
        sixel_status_dispose_message(&message);
        if (strcmp(expected, sixel_helper_get_additional_message()) != 0) {
            return 0;
        }
    }
    sixel_helper_set_additional_message(NULL);
    return 1;
}

static int
lookup_failure_encode(char const *threads, char const *precision,
                      char const *lookup, int mode, char const *mapfile)
{
    sixel_encoder_t *encoder;
    sixel_frame_t *frame;
    sixel_output_t *output;
    SIXELSTATUS status;
    unsigned char pixels[32 * 32 * 3];
    int written;
    int i;
    int ok;

    encoder = NULL;
    frame = NULL;
    output = NULL;
    written = 0;
    ok = 0;
    for (i = 0; i < (int)sizeof(pixels); i++) {
        pixels[i] = (unsigned char)(i * 17 + i / 31);
    }
    status = sixel_encoder_new(&encoder, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (SIXEL_FAILED(sixel_encoder_setopt(encoder,
                                         SIXEL_OPTFLAG_THREADING_POLICY,
                                         threads)) ||
        SIXEL_FAILED(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_PRECISION_POLICY,
                                         precision)) ||
        SIXEL_FAILED(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_LUT_POLICY,
                                         lookup)) ||
        SIXEL_FAILED(sixel_encoder_setopt(encoder,
                                         SIXEL_OPTFLAG_SAMPLING_POLICY,
                                         "auto")) ||
        SIXEL_FAILED(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_GPU_POLICY,
                                         "off"))) {
        goto end;
    }
    if (mode == 1 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
            SIXEL_OPTFLAG_BUILTIN_PALETTE, "xterm256"))) {
        goto end;
    }
    if (mode == 2 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
            SIXEL_OPTFLAG_MAPFILE, mapfile))) {
        goto end;
    }
    status = sixel_frame_new(&frame, NULL);
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_frame_init_borrowed(frame, pixels, 32, 32,
                                           SIXEL_PIXELFORMAT_RGB888,
                                           NULL, 0);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_output_new(&output, lookup_failure_write, &written,
                                  NULL);
    }
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    sixel_helper_set_additional_message("stale caller diagnostic");
    status = sixel_encoder_encode_frame(encoder, frame, output);
    ok = status == SIXEL_BAD_ARGUMENT && written == 0 &&
        strcmp(sixel_helper_get_additional_message(),
               "Injected palette lookup preparation failure.") == 0;
    if (!ok) {
        fprintf(stderr, "threads=%s precision=%s lookup=%s mode=%d "
                "status=%d message=%s\n", threads, precision, lookup, mode,
                status, sixel_helper_get_additional_message());
    }
end:
    sixel_output_unref(output);
    sixel_frame_unref(frame);
    sixel_encoder_unref(encoder);
    return ok;
}

int
test_palette_0081_lookup_failure(int argc, char **argv)
{
    char const *threads[] = {"1", "2", "4", "8"};
    char const *precision[] = {"8bit", "float32"};
    char const *lookup[] = {"fhedt", "vptree"};
    int t;
    int p;
    int l;
    int mode;

    if (argc != 2 || !lookup_failure_message()) {
        return EXIT_FAILURE;
    }
    for (t = 0; t < 4; t++) {
        for (p = 0; p < 2; p++) {
            for (l = 0; l < 2; l++) {
                for (mode = 0; mode < 3; mode++) {
                    if (!lookup_failure_encode(threads[t], precision[p],
                                                lookup[l], mode, argv[1])) {
                        return EXIT_FAILURE;
                    }
                }
            }
        }
    }
    return EXIT_SUCCESS;
}
