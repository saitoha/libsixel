/*
 * SPDX-License-Identifier: MIT
 *
 * Minimal OpenVMS smoke program for the native static-library bootstrap.
 */

#include <stdio.h>

#include <sixel.h>

typedef struct smoke_writer {
    FILE *fp;
} smoke_writer_t;

static int
smoke_write(char *data, int size, void *priv)
{
    smoke_writer_t *writer;
    size_t written;

    writer = (smoke_writer_t *)priv;
    if (writer == NULL || writer->fp == NULL || size < 0) {
        return (-1);
    }

    written = fwrite(data, 1u, (size_t)size, writer->fp);
    if (written != (size_t)size) {
        return (-1);
    }

    return size;
}

int
main(void)
{
    static unsigned char pixels[] = {
        0xff, 0x00, 0x00, 0x00, 0xff, 0x00,
        0x00, 0x00, 0xff, 0xff, 0xff, 0x00
    };
    FILE *fp;
    smoke_writer_t writer;
    sixel_output_t *output;
    sixel_dither_t *dither;
    SIXELSTATUS status;
    int result;

    fp = NULL;
    writer.fp = NULL;
    output = NULL;
    dither = NULL;
    status = SIXEL_FALSE;
    result = 1;

    fp = fopen("[.openvms.obj]sixel_smoke.six", "wb");
    if (fp == NULL) {
        fprintf(stderr, "failed to open sixel_smoke.six\n");
        goto end;
    }
    writer.fp = fp;

    status = sixel_output_new(&output, smoke_write, &writer, NULL);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "sixel_output_new failed: %d\n", status);
        goto end;
    }

    dither = sixel_dither_get(SIXEL_BUILTIN_XTERM256);
    if (dither == NULL) {
        fprintf(stderr, "sixel_dither_get failed\n");
        goto end;
    }
    sixel_dither_set_pixelformat(dither, SIXEL_PIXELFORMAT_RGB888);

    status = sixel_encode(pixels, 2, 2, 3, dither, output);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "sixel_encode failed: %d\n", status);
        goto end;
    }

    result = 0;

end:
    if (output != NULL) {
        sixel_output_unref(output);
    }
    if (fp != NULL) {
        fclose(fp);
    }

    if (result == 0) {
        printf("wrote [.openvms.obj]sixel_smoke.six\n");
    }

    return result;
}
