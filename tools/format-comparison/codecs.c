/* In-memory adapters for the format study; no terminal or PNG output I/O. */
#include <sixel.h>
#include <webp/encode.h>
#include <webp/decode.h>
#include <stdio.h>

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct buffer {
    unsigned char *data;
    size_t size;
    size_t capacity;
} buffer_t;

static int
append(char *data, int size, void *context)
{
    buffer_t *buffer;
    unsigned char *next;
    size_t capacity;

    buffer = context;
    if (size < 0 || (size_t)size > SIZE_MAX - buffer->size) {
        return -1;
    }
    if (buffer->size + (size_t)size > buffer->capacity) {
        capacity = buffer->size + (size_t)size;
        if (capacity <= SIZE_MAX / 2) {
            capacity *= 2;
        }
        next = realloc(buffer->data, capacity);
        if (next == NULL) {
            return -1;
        }
        buffer->data = next;
        buffer->capacity = capacity;
    }
    memcpy(buffer->data + buffer->size, data, (size_t)size);
    buffer->size += (size_t)size;
    return size;
}

/* Return malloc-owned bytes. The Python driver always calls study_free. */
void
study_free(void *data)
{
    free(data);
}

int
study_sixel_encode(unsigned char *pixels, int width, int height,
                   unsigned char *palette, int colors, int threads,
                   int no_dither, unsigned char **data, size_t *size)
{
    sixel_encoder_t *encoder;
    sixel_frame_t *frame;
    sixel_output_t *output;
    buffer_t buffer;
    SIXELSTATUS status;
    char count[32];

    encoder = NULL;
    frame = NULL;
    output = NULL;
    memset(&buffer, 0, sizeof(buffer));
    snprintf(count, sizeof(count), "%d", threads);
    status = sixel_encoder_new(&encoder, NULL);
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_encoder_setopt(encoder, '=', count);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_encoder_setopt(encoder, '.', "8bit");
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_encoder_setopt(encoder, 'G', "off");
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_encoder_setopt(encoder, 'E', "fast");
    }
    if (SIXEL_SUCCEEDED(status) && no_dither) {
        status = sixel_encoder_setopt(encoder, 'd', "none");
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_frame_new(&frame, NULL);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_frame_init_borrowed(frame, pixels, width, height,
                    palette ? SIXEL_PIXELFORMAT_PAL8 : SIXEL_PIXELFORMAT_RGB888,
                    palette, palette ? colors : -1);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_output_new(&output, append, &buffer, NULL);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_encoder_encode_frame(encoder, frame, output);
    }
    if (output != NULL) sixel_output_unref(output);
    if (frame != NULL) sixel_frame_unref(frame);
    if (encoder != NULL) sixel_encoder_unref(encoder);
    if (SIXEL_FAILED(status)) {
        free(buffer.data);
        return 0;
    }
    *data = buffer.data;
    *size = buffer.size;
    return 1;
}

int
study_webp_encode(unsigned char *rgb, int width, int height, int quality,
                  int lossless, int threads, unsigned char **data, size_t *size)
{
    WebPConfig config;
    WebPPicture picture;
    WebPMemoryWriter writer;
    int ok;

    if (!WebPConfigInit(&config) || !WebPPictureInit(&picture)) return 0;
    config.quality = (float)quality;
    config.lossless = lossless;
    config.method = 4;
    /* libwebp exposes a Boolean switch, not an exact worker count. */
    config.thread_level = threads > 1;
    picture.width = width;
    picture.height = height;
    picture.use_argb = lossless;
    WebPMemoryWriterInit(&writer);
    picture.writer = WebPMemoryWrite;
    picture.custom_ptr = &writer;
    ok = WebPPictureImportRGB(&picture, rgb, width * 3);
    if (ok) ok = WebPEncode(&config, &picture);
    *data = NULL;
    if (ok) {
        *data = malloc(writer.size);
        ok = *data != NULL;
        if (ok) {
            memcpy(*data, writer.mem, writer.size);
            *size = writer.size;
        }
    }
    WebPPictureFree(&picture);
    WebPMemoryWriterClear(&writer);
    return ok;
}

int
study_webp_decode(unsigned char *data, size_t size, int threads,
                  unsigned char **rgb, int *width, int *height)
{
    WebPDecoderConfig config;
    size_t bytes;
    int ok;

    if (!WebPInitDecoderConfig(&config)) return 0;
    config.output.colorspace = MODE_RGB;
    config.options.use_threads = threads > 1;
    ok = WebPDecode(data, size, &config) == VP8_STATUS_OK;
    *rgb = NULL;
    if (ok) {
        *width = config.output.width;
        *height = config.output.height;
        bytes = (size_t)*width * (size_t)*height * 3;
        *rgb = malloc(bytes);
        ok = *rgb != NULL;
        if (ok) memcpy(*rgb, config.output.u.RGBA.rgba, bytes);
    }
    WebPFreeDecBuffer(&config.output);
    return ok;
}
