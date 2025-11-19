#ifndef SIXEL_DECODER_IMAGE_H
#define SIXEL_DECODER_IMAGE_H

#include <sixel.h>

#ifndef SIXEL_PALETTE_MAX_DECODER
#define SIXEL_PALETTE_MAX_DECODER 65536
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct image_buffer {
    union {
        void *p;
        unsigned char *in_bytes;
        unsigned short *in_shorts;
    } pixels;
    int width;
    int height;
    int depth;
    int ncolors;
    int palette[SIXEL_PALETTE_MAX_DECODER];
} image_buffer_t;

SIXELSTATUS image_buffer_init(image_buffer_t *image,
                              int width,
                              int height,
                              int bgindex,
                              int depth,
                              sixel_allocator_t *allocator);

SIXELSTATUS image_buffer_resize(image_buffer_t *image,
                                int width,
                                int height,
                                int bgindex,
                                sixel_allocator_t *allocator);

#ifdef __cplusplus
}
#endif

#endif /* SIXEL_DECODER_IMAGE_H */
