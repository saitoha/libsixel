/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 */

#if defined(HAVE_CONFIG_H)
# include "config.h"
#endif

#include <stddef.h>
#include <stdint.h>

#include "fuzz-loader-builtin-struct-common.h"

static int
fuzz_append_psd_resource(fuzz_byte_buffer_t *resources,
                         uint16_t resource_id,
                         unsigned char const *name,
                         size_t name_size,
                         unsigned char const *data,
                         size_t data_size)
{
    unsigned char pascal_len;

    if (resources == NULL || name == NULL || data == NULL) {
        return 0;
    }
    if (name_size > 255u || data_size > UINT32_MAX) {
        return 0;
    }

    pascal_len = (unsigned char)name_size;
    if (!fuzz_byte_buffer_append(resources,
                                 (unsigned char const *)"8BIM",
                                 4u) ||
        !fuzz_byte_buffer_append_u16be(resources, resource_id) ||
        !fuzz_byte_buffer_append_u8(resources, pascal_len) ||
        !fuzz_byte_buffer_append(resources, name, name_size)) {
        return 0;
    }

    if (((1u + name_size) & 0x01u) != 0u) {
        if (!fuzz_byte_buffer_append_u8(resources, 0u)) {
            return 0;
        }
    }

    if (!fuzz_byte_buffer_append_u32be(resources, (uint32_t)data_size) ||
        !fuzz_byte_buffer_append(resources, data, data_size)) {
        return 0;
    }

    if ((data_size & 0x01u) != 0u) {
        if (!fuzz_byte_buffer_append_u8(resources, 0u)) {
            return 0;
        }
    }

    return 1;
}

static int
fuzz_build_psd_resource_payload(uint8_t const *data,
                                size_t size,
                                fuzz_byte_buffer_t *payload)
{
    static unsigned char const default_name[] = { 'I', 'C', 'C' };
    static unsigned char const fallback_icc[32] = {
        0x00u, 0x00u, 0x02u, 0x10u, 0x6cu, 0x63u, 0x6du, 0x73u,
        0x04u, 0x20u, 0x00u, 0x00u, 0x6du, 0x6e, 0x74u, 0x72u,
        0x52u, 0x47u, 0x42u, 0x20u, 0x58u, 0x59u, 0x5au, 0x20u,
        0x07u, 0xe8u, 0x00u, 0x02u, 0x00u, 0x01u, 0x00u, 0x00u
    };

    fuzz_cursor_t cursor;
    fuzz_byte_buffer_t resources;
    uint16_t channels;
    uint32_t height;
    uint32_t width;
    uint16_t depth;
    uint16_t mode;
    size_t resource_count;
    size_t i;

    if (payload == NULL) {
        return 0;
    }

    fuzz_cursor_init(&cursor, data, size);
    fuzz_byte_buffer_init(&resources);

    channels = (uint16_t)(1u + (fuzz_cursor_take_u8(&cursor, 0u) % 8u));
    width = 1u + (uint32_t)(fuzz_cursor_take_u16be(&cursor, 1u) % 2048u);
    height = 1u + (uint32_t)(fuzz_cursor_take_u16be(&cursor, 1u) % 2048u);

    if ((fuzz_cursor_take_u8(&cursor, 0u) & 0x01u) != 0u) {
        width = (uint32_t)fuzz_cursor_take_u32be(&cursor, width);
        height = (uint32_t)fuzz_cursor_take_u32be(&cursor, height);
    }

    depth = (uint16_t[]){1u, 8u, 16u, 32u}[fuzz_cursor_take_u8(&cursor, 1u) % 4u];
    mode = (uint16_t[]){0u, 1u, 2u, 3u, 4u, 7u, 8u, 9u}[fuzz_cursor_take_u8(&cursor, 3u) % 8u];

    resource_count = 1u + (size_t)(fuzz_cursor_take_u8(&cursor, 0u) % 4u);
    for (i = 0u; i < resource_count; ++i) {
        uint16_t resource_id;
        unsigned char const *resource_data;
        size_t resource_size;
        unsigned char name_bytes[8];
        size_t name_size;

        if (i == 0u || (fuzz_cursor_take_u8(&cursor, 0u) & 0x01u) == 0u) {
            resource_id = 0x040fu;
        } else {
            resource_id = (uint16_t)(fuzz_cursor_take_u16be(&cursor, 0u));
            if (resource_id == 0u) {
                resource_id = 0x03efu;
            }
        }

        name_size = (size_t)(fuzz_cursor_take_u8(&cursor, 0u) % sizeof(name_bytes));
        if (name_size == 0u) {
            name_size = sizeof(default_name);
            name_bytes[0] = default_name[0];
            name_bytes[1] = default_name[1];
            name_bytes[2] = default_name[2];
        } else {
            size_t j;
            for (j = 0u; j < name_size; ++j) {
                name_bytes[j] = fuzz_cursor_take_u8(&cursor,
                                                    (unsigned char)('A' + (j % 26u)));
            }
        }

        resource_data = fallback_icc;
        resource_size = sizeof(fallback_icc);
        if (fuzz_cursor_remaining(&cursor) > 0u) {
            resource_size = (size_t)(fuzz_cursor_take_u16be(&cursor, 0u) % 513u);
            if (resource_size > fuzz_cursor_remaining(&cursor)) {
                resource_size = fuzz_cursor_remaining(&cursor);
            }
            if (resource_size > 0u) {
                resource_data = cursor.data + cursor.pos;
                cursor.pos += resource_size;
            } else {
                resource_data = fallback_icc;
                resource_size = sizeof(fallback_icc);
            }
        }

        if (!fuzz_append_psd_resource(&resources,
                                      resource_id,
                                      name_bytes,
                                      name_size,
                                      resource_data,
                                      resource_size)) {
            fuzz_byte_buffer_reset(&resources);
            return 0;
        }
    }

    if (!fuzz_byte_buffer_append(payload,
                                 (unsigned char const *)"8BPS",
                                 4u) ||
        !fuzz_byte_buffer_append_u16be(payload, 1u) ||
        !fuzz_byte_buffer_append_zeros(payload, 6u) ||
        !fuzz_byte_buffer_append_u16be(payload, channels) ||
        !fuzz_byte_buffer_append_u32be(payload, height) ||
        !fuzz_byte_buffer_append_u32be(payload, width) ||
        !fuzz_byte_buffer_append_u16be(payload, depth) ||
        !fuzz_byte_buffer_append_u16be(payload, mode) ||
        !fuzz_byte_buffer_append_u32be(payload, 0u) ||
        !fuzz_byte_buffer_append_u32be(payload, (uint32_t)resources.size) ||
        !fuzz_byte_buffer_append(payload, resources.data, resources.size) ||
        !fuzz_byte_buffer_append_u32be(payload, 0u) ||
        !fuzz_byte_buffer_append_u16be(payload,
                                       (uint16_t)(fuzz_cursor_take_u8(&cursor, 0u) % 3u))) {
        fuzz_byte_buffer_reset(&resources);
        return 0;
    }

    if (fuzz_cursor_remaining(&cursor) > 0u) {
        if (!fuzz_byte_buffer_append(payload,
                                     cursor.data + cursor.pos,
                                     fuzz_cursor_remaining(&cursor))) {
            fuzz_byte_buffer_reset(&resources);
            return 0;
        }
    } else if (!fuzz_byte_buffer_append_zeros(payload, 8u)) {
        fuzz_byte_buffer_reset(&resources);
        return 0;
    }

    fuzz_byte_buffer_reset(&resources);
    return 1;
}

int
LLVMFuzzerInitialize(int *argc, char ***argv)
{
    (void)argc;
    (void)argv;
    (void)fuzz_loader_builtin_runtime_bootstrap();
    return 0;
}

int
LLVMFuzzerTestOneInput(uint8_t const *data, size_t size)
{
    enum { FUZZ_MAX_INPUT_BYTES = 1 * 1024 * 1024 };

    fuzz_byte_buffer_t payload;

    if (data == NULL || size > (size_t)FUZZ_MAX_INPUT_BYTES) {
        return 0;
    }
    if (!fuzz_loader_builtin_runtime_bootstrap()) {
        return 0;
    }

    fuzz_byte_buffer_init(&payload);
    if (!fuzz_build_psd_resource_payload(data, size, &payload)) {
        fuzz_byte_buffer_reset(&payload);
        return 0;
    }

    (void)fuzz_loader_builtin_runtime_run(data,
                                          size,
                                          payload.data,
                                          payload.size);
    fuzz_byte_buffer_reset(&payload);
    return 0;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
