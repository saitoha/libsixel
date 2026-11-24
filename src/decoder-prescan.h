/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_DECODER_PRESCAN_H
#define LIBSIXEL_DECODER_PRESCAN_H

#include <stdint.h>

#include <sixel.h>

#define SIXEL_PRESCAN_CUT_SHIFT 30u
#define SIXEL_PRESCAN_CUT_OFFSET ((UINT32_C(1) << SIXEL_PRESCAN_CUT_SHIFT) - 1u)
#define SIXEL_PRESCAN_CUT_GRAPHIC 0u
#define SIXEL_PRESCAN_CUT_CR 1u
#define SIXEL_PRESCAN_CUT_NL 2u
#define SIXEL_PRESCAN_CHUNK_BYTES 64u

/* Offsets count from just after the DCS final byte. */
SIXEL_INTERNAL_API uint32_t const *
sixel_decode_prescan_cuts(sixel_decode_prescan_t const *plan,
                         size_t payload_size, size_t *count);

#endif
