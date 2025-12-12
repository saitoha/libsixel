/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 */
#ifndef SIXEL_PROBE_H
#define SIXEL_PROBE_H

#include <stddef.h>
#include <stdint.h>

#include <sixel.h>

SIXELAPI SIXELSTATUS
sixel_probe_find_dcs_start(uint8_t const *data,
                           size_t len,
                           size_t *out_offset);

SIXELAPI SIXELSTATUS
sixel_probe_is_probable(uint8_t const *data, size_t len);

SIXELAPI SIXELSTATUS
sixel_parse_header(unsigned char const *ibuf,
                   size_t headsize,
                   unsigned int **pparams,
                   size_t *pparamsize,
                   sixel_allocator_t *allocator);

#endif /* SIXEL_PROBE_H */
