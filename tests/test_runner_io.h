/*
 * SPDX-License-Identifier: MIT
 *
 * Test-runner-local stdio helpers.
 */

#ifndef LIBSIXEL_TEST_RUNNER_IO_H
#define LIBSIXEL_TEST_RUNNER_IO_H

#include <stdio.h>

/*
 * A FILE stream must be created, consumed, and closed by the same CRT module.
 * In particular, a native Windows test executable must not consume a stream
 * created by the libsixel DLL through sixel_compat_fopen().
 */
static FILE *
test_runner_fopen(char const *path, char const *mode)
{
    FILE *stream;

    stream = NULL;
#if defined(_MSC_VER)
    if (fopen_s(&stream, path, mode) != 0) {
        stream = NULL;
    }
#else
    stream = fopen(path, mode);
#endif
    return stream;
}

#endif /* LIBSIXEL_TEST_RUNNER_IO_H */
