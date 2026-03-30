/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 */

#ifndef SIXEL_FUZZ_LOADER_BUILTIN_STRUCT_COMMON_H
#define SIXEL_FUZZ_LOADER_BUILTIN_STRUCT_COMMON_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct fuzz_byte_buffer {
    unsigned char *data;
    size_t size;
    size_t capacity;
} fuzz_byte_buffer_t;

typedef struct fuzz_cursor {
    uint8_t const *data;
    size_t size;
    size_t pos;
} fuzz_cursor_t;

void fuzz_loader_builtin_runtime_shutdown(void);
int fuzz_loader_builtin_runtime_bootstrap(void);
int fuzz_loader_builtin_runtime_run(uint8_t const *option_data,
                                    size_t option_size,
                                    unsigned char const *payload,
                                    size_t payload_size);

static inline void
fuzz_byte_buffer_init(fuzz_byte_buffer_t *buffer)
{
    if (buffer == NULL) {
        return;
    }
    buffer->data = NULL;
    buffer->size = 0u;
    buffer->capacity = 0u;
}

static inline void
fuzz_byte_buffer_reset(fuzz_byte_buffer_t *buffer)
{
    if (buffer == NULL) {
        return;
    }
    free(buffer->data);
    buffer->data = NULL;
    buffer->size = 0u;
    buffer->capacity = 0u;
}

static inline int
fuzz_byte_buffer_reserve(fuzz_byte_buffer_t *buffer, size_t need)
{
    unsigned char *next;
    size_t next_capacity;

    if (buffer == NULL) {
        return 0;
    }
    if (need <= buffer->capacity) {
        return 1;
    }

    next_capacity = buffer->capacity == 0u ? 256u : buffer->capacity;
    while (next_capacity < need) {
        if (next_capacity > ((size_t)-1) / 2u) {
            next_capacity = need;
            break;
        }
        next_capacity *= 2u;
    }

    next = (unsigned char *)realloc(buffer->data, next_capacity);
    if (next == NULL) {
        return 0;
    }

    buffer->data = next;
    buffer->capacity = next_capacity;
    return 1;
}

static inline int
fuzz_byte_buffer_append(fuzz_byte_buffer_t *buffer,
                        unsigned char const *data,
                        size_t length)
{
    size_t next_size;

    if (buffer == NULL) {
        return 0;
    }
    if (length == 0u) {
        return 1;
    }
    if (data == NULL) {
        return 0;
    }
    if (buffer->size > (size_t)-1 - length) {
        return 0;
    }

    next_size = buffer->size + length;
    if (!fuzz_byte_buffer_reserve(buffer, next_size)) {
        return 0;
    }

    memcpy(buffer->data + buffer->size, data, length);
    buffer->size = next_size;
    return 1;
}

static inline int
fuzz_byte_buffer_append_u8(fuzz_byte_buffer_t *buffer, unsigned char value)
{
    return fuzz_byte_buffer_append(buffer, &value, 1u);
}

static inline int
fuzz_byte_buffer_append_u16be(fuzz_byte_buffer_t *buffer, uint16_t value)
{
    unsigned char raw[2];

    raw[0] = (unsigned char)((value >> 8) & 0xffu);
    raw[1] = (unsigned char)(value & 0xffu);
    return fuzz_byte_buffer_append(buffer, raw, sizeof(raw));
}

static inline int
fuzz_byte_buffer_append_u16le(fuzz_byte_buffer_t *buffer, uint16_t value)
{
    unsigned char raw[2];

    raw[0] = (unsigned char)(value & 0xffu);
    raw[1] = (unsigned char)((value >> 8) & 0xffu);
    return fuzz_byte_buffer_append(buffer, raw, sizeof(raw));
}

static inline int
fuzz_byte_buffer_append_u32be(fuzz_byte_buffer_t *buffer, uint32_t value)
{
    unsigned char raw[4];

    raw[0] = (unsigned char)((value >> 24) & 0xffu);
    raw[1] = (unsigned char)((value >> 16) & 0xffu);
    raw[2] = (unsigned char)((value >> 8) & 0xffu);
    raw[3] = (unsigned char)(value & 0xffu);
    return fuzz_byte_buffer_append(buffer, raw, sizeof(raw));
}

static inline int
fuzz_byte_buffer_append_u32le(fuzz_byte_buffer_t *buffer, uint32_t value)
{
    unsigned char raw[4];

    raw[0] = (unsigned char)(value & 0xffu);
    raw[1] = (unsigned char)((value >> 8) & 0xffu);
    raw[2] = (unsigned char)((value >> 16) & 0xffu);
    raw[3] = (unsigned char)((value >> 24) & 0xffu);
    return fuzz_byte_buffer_append(buffer, raw, sizeof(raw));
}

static inline int
fuzz_byte_buffer_append_zeros(fuzz_byte_buffer_t *buffer, size_t length)
{
    static unsigned char const zeros[64] = {0};
    size_t remaining;
    size_t chunk;

    if (buffer == NULL) {
        return 0;
    }

    remaining = length;
    while (remaining > 0u) {
        chunk = remaining;
        if (chunk > sizeof(zeros)) {
            chunk = sizeof(zeros);
        }
        if (!fuzz_byte_buffer_append(buffer, zeros, chunk)) {
            return 0;
        }
        remaining -= chunk;
    }
    return 1;
}

static inline void
fuzz_cursor_init(fuzz_cursor_t *cursor, uint8_t const *data, size_t size)
{
    if (cursor == NULL) {
        return;
    }
    cursor->data = data;
    cursor->size = size;
    cursor->pos = 0u;
}

static inline size_t
fuzz_cursor_remaining(fuzz_cursor_t const *cursor)
{
    if (cursor == NULL || cursor->pos >= cursor->size) {
        return 0u;
    }
    return cursor->size - cursor->pos;
}

static inline unsigned char
fuzz_cursor_take_u8(fuzz_cursor_t *cursor, unsigned char fallback)
{
    if (cursor == NULL || cursor->data == NULL || cursor->pos >= cursor->size) {
        return fallback;
    }
    return cursor->data[cursor->pos++];
}

static inline uint16_t
fuzz_cursor_take_u16be(fuzz_cursor_t *cursor, uint16_t fallback)
{
    uint16_t value;

    if (fuzz_cursor_remaining(cursor) < 2u) {
        return fallback;
    }

    value = (uint16_t)cursor->data[cursor->pos] << 8;
    value |= (uint16_t)cursor->data[cursor->pos + 1u];
    cursor->pos += 2u;
    return value;
}

static inline uint32_t
fuzz_cursor_take_u32be(fuzz_cursor_t *cursor, uint32_t fallback)
{
    uint32_t value;

    if (fuzz_cursor_remaining(cursor) < 4u) {
        return fallback;
    }

    value = (uint32_t)cursor->data[cursor->pos] << 24;
    value |= (uint32_t)cursor->data[cursor->pos + 1u] << 16;
    value |= (uint32_t)cursor->data[cursor->pos + 2u] << 8;
    value |= (uint32_t)cursor->data[cursor->pos + 3u];
    cursor->pos += 4u;
    return value;
}

static inline int
fuzz_cursor_take_span(fuzz_cursor_t *cursor,
                      size_t max_length,
                      unsigned char const **out_data,
                      size_t *out_length)
{
    size_t remaining;
    size_t span;

    if (out_data == NULL || out_length == NULL) {
        return 0;
    }

    *out_data = NULL;
    *out_length = 0u;

    if (cursor == NULL || cursor->data == NULL || cursor->pos >= cursor->size) {
        return 1;
    }

    remaining = cursor->size - cursor->pos;
    span = fuzz_cursor_take_u8(cursor, 0u);
    if (max_length > 0u && span > max_length) {
        span = max_length;
    }
    if (span > remaining - 1u) {
        span = remaining - 1u;
    }

    *out_data = cursor->data + cursor->pos;
    *out_length = span;
    cursor->pos += span;
    return 1;
}

static inline uint32_t
fuzz_crc32(unsigned char const *data, size_t length)
{
    uint32_t crc;
    size_t index;
    size_t bit;

    crc = 0xffffffffu;
    for (index = 0u; index < length; ++index) {
        crc ^= (uint32_t)data[index];
        for (bit = 0u; bit < 8u; ++bit) {
            if ((crc & 1u) != 0u) {
                crc = (crc >> 1) ^ 0xedb88320u;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc ^ 0xffffffffu;
}

static inline int
fuzz_append_png_chunk(fuzz_byte_buffer_t *buffer,
                      char const type[4],
                      unsigned char const *data,
                      size_t length,
                      int force_bad_crc,
                      uint32_t crc_salt)
{
    fuzz_byte_buffer_t crc_buffer;
    uint32_t crc;

    if (buffer == NULL || type == NULL) {
        return 0;
    }
    if (length > UINT32_MAX) {
        return 0;
    }
    if (length > 0u && data == NULL) {
        return 0;
    }

    if (!fuzz_byte_buffer_append_u32be(buffer, (uint32_t)length)) {
        return 0;
    }
    if (!fuzz_byte_buffer_append(buffer,
                                 (unsigned char const *)(uintptr_t)type,
                                 4u)) {
        return 0;
    }
    if (!fuzz_byte_buffer_append(buffer, data, length)) {
        return 0;
    }

    fuzz_byte_buffer_init(&crc_buffer);
    if (!fuzz_byte_buffer_append(&crc_buffer,
                                 (unsigned char const *)(uintptr_t)type,
                                 4u)) {
        fuzz_byte_buffer_reset(&crc_buffer);
        return 0;
    }
    if (!fuzz_byte_buffer_append(&crc_buffer, data, length)) {
        fuzz_byte_buffer_reset(&crc_buffer);
        return 0;
    }

    crc = fuzz_crc32(crc_buffer.data, crc_buffer.size);
    fuzz_byte_buffer_reset(&crc_buffer);
    if (force_bad_crc) {
        crc ^= crc_salt;
    }

    return fuzz_byte_buffer_append_u32be(buffer, crc);
}

#endif
