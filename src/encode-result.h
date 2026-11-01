/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_ENCODE_RESULT_H
#define LIBSIXEL_ENCODE_RESULT_H

#include <6cells.h>
#include "sixel_atomic.h"

/* Private producer access. Published rows are immutable; only the sole
 * producer can fill storage before publication. Consumers retain the owner. */
struct sixel_indexed_frame {
    sixel_atomic_u32_t ref;
    sixel_allocator_t *allocator;
    sixel_indexed_frame_description_t description;
    sixel_encode_identity_t identity;
    sixel_index_t *indexes;
    unsigned char *skip;
    size_t pixels;
    sixel_atomic_i32_t rows;
    sixel_atomic_i32_t state;
    sixel_retained_update_t *update; /* Pinned by each active resource user. */
    sixel_atomic_u32_t users;
};

struct sixel_stream_receipt {
    sixel_atomic_u32_t ref;
    sixel_allocator_t *allocator;
    sixel_encode_identity_t identity;
};

SIXEL_INTERNAL_API void
sixel_indexed_frame_acquire_user(sixel_indexed_frame_t *image);
SIXEL_INTERNAL_API void
sixel_indexed_frame_release_user(sixel_indexed_frame_t *image);
SIXEL_INTERNAL_API SIXELSTATUS
sixel_indexed_frame_publish(sixel_indexed_frame_t *image, int rows);
SIXEL_INTERNAL_API SIXELSTATUS
sixel_indexed_frame_publish_applied(sixel_indexed_frame_t *image, int rows);
/* Only the fixed GPU kernel may use this publication: it already restricts
 * every painted index and writes canonical final skip codes. Imported or
 * pluggable-policy output must use the validating publication path. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_indexed_frame_publish_gpu(sixel_indexed_frame_t *image);
SIXEL_INTERNAL_API void
sixel_indexed_frame_fail(sixel_indexed_frame_t *image);
SIXEL_INTERNAL_API SIXELSTATUS
sixel_indexed_frame_complete(sixel_indexed_frame_t *image);
SIXEL_INTERNAL_API SIXELSTATUS
sixel_stream_receipt_new(sixel_stream_receipt_t **result,
    sixel_encode_identity_t const *identity, sixel_allocator_t *allocator);

/* @classid image/retained-plane */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_retained_plane_factory_new(sixel_allocator_t *allocator, void **object);

/* @classid processing/palette-apply */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_apply_factory_new(sixel_allocator_t *allocator, void **object);

/* Only the serialized legacy adapter may use this paired mutable borrow. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_retained_plane_legacy_map(sixel_retained_plane_interface_t *self,
    unsigned char **rgb, unsigned char **valid);
SIXEL_INTERNAL_API void
sixel_retained_plane_legacy_unmap(sixel_retained_plane_interface_t *self);

#endif
