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
#include <string.h>

#include "fuzz-loader-builtin-struct-common.h"

static int
fuzz_build_pic_structure_payload(uint8_t const *data,
                                 size_t size,
                                 fuzz_byte_buffer_t *payload)
{
    fuzz_cursor_t cursor;
    size_t target_size;
    size_t body_size;

    if (payload == NULL) {
        return 0;
    }

    fuzz_cursor_init(&cursor, data, size);
    target_size = 128u + (size_t)(fuzz_cursor_take_u16be(&cursor, 0u) % 2048u);
    if (target_size < 96u) {
        target_size = 96u;
    }

    if (!fuzz_byte_buffer_append_zeros(payload, target_size)) {
        return 0;
    }

    payload->data[0] = 0x53u;
    payload->data[1] = 0x80u;
    payload->data[2] = 0xf6u;
    payload->data[3] = 0x34u;
    payload->data[88] = 'P';
    payload->data[89] = 'I';
    payload->data[90] = 'C';
    payload->data[91] = 'T';

    payload->data[4] = fuzz_cursor_take_u8(&cursor, 0u);
    payload->data[5] = fuzz_cursor_take_u8(&cursor, 0u);
    payload->data[6] = fuzz_cursor_take_u8(&cursor, 0u);
    payload->data[7] = fuzz_cursor_take_u8(&cursor, 0u);

    body_size = target_size - 92u;
    if (body_size > fuzz_cursor_remaining(&cursor)) {
        body_size = fuzz_cursor_remaining(&cursor);
    }
    if (body_size > 0u) {
        memcpy(payload->data + 92u,
               cursor.data + cursor.pos,
               body_size);
    }

    if (target_size >= 96u) {
        payload->data[target_size - 4u] = 0x00u;
        payload->data[target_size - 3u] = 0xffu;
        payload->data[target_size - 2u] = 0x00u;
        payload->data[target_size - 1u] = 0xffu;
    }

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
    if (!fuzz_build_pic_structure_payload(data, size, &payload)) {
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
