/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_DECODER_GPU_H
#define LIBSIXEL_DECODER_GPU_H

#include <stddef.h>
#include <stdint.h>

#include <sixel.h>

#include "timeline-logger.h"

/* Under the AUTO GPU policy, bodies shorter than this decode faster on the
 * CPU workers than a device submission takes to complete; FORCE tries the
 * device for any body. */
#define SIXEL_DECODER_GPU_MIN_BODY 65536u

#if defined(HAVE_METAL)
/*
 * Parse an ordinary overwrite-mode body into band-column words on the device.
 * INPUT[BODY, LENGTH) is the body after the raster and the leading palette
 * definitions; INITIAL_COLOR is the selection the CPU workers start from.
 * COLUMNS receives ceil(HEIGHT / 6) bands of WIDTH words, exactly as the
 * parallel gather would write them.  On SIXEL_OK, ALPHA_OPAQUE is 1 or 0 when
 * every band was reached, otherwise -1, and PAINTED_OUTSIDE_RASTER reports
 * clipped paint.  SIXEL_FALSE leaves COLUMNS unspecified and means the body
 * needs the CPU parser: malformed or unusual input, or a busy device.
 */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_decoder_gpu_parse_columns(unsigned char const *input,
                                size_t length,
                                size_t body,
                                int width,
                                int height,
                                int initial_color,
                                int trust_raster_size,
                                uint64_t *columns,
                                int *alpha_opaque,
                                int *painted_outside_raster,
                                sixel_decode_prescan_t const *plan,
                                sixel_timeline_logger_t *logger);
#endif

#endif
