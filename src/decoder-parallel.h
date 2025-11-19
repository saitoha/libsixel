#ifndef SIXEL_DECODER_PARALLEL_H
#define SIXEL_DECODER_PARALLEL_H

#include <sixel.h>

#include "decoder-image.h"

#ifdef __cplusplus
extern "C" {
#endif

SIXELSTATUS sixel_decode_raw_parallel(unsigned char *p,
                                      int len,
                                      image_buffer_t *image,
                                      sixel_allocator_t *allocator,
                                      int *used_parallel);

SIXELSTATUS sixel_decode_direct_parallel(unsigned char *p,
                                         int len,
                                         image_buffer_t *image,
                                         sixel_allocator_t *allocator,
                                         int *used_parallel);

int sixel_decoder_parallel_resolve_threads(void);
SIXELSTATUS sixel_decoder_parallel_override_threads(char const *text);

#ifdef __cplusplus
}
#endif

#endif /* SIXEL_DECODER_PARALLEL_H */
