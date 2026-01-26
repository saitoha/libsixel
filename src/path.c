/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if HAVE_CTYPE_H
#include <ctype.h>
#endif  /* HAVE_CTYPE_H */
#if HAVE_STRING_H
#include <string.h>
#endif  /* HAVE_STRING_H */

#include "compat_stub.h"
#include "path.h"

#define SIXEL_CYGDRIVE_PREFIX "/cygdrive/"

#if defined(_WIN32) || defined(__CYGWIN__)
static int
sixel_path_is_unc(char const *path)
{
    if (path == NULL) {
        return 0;
    }

    return path[0] == '/' && path[1] == '/';
}

static int
sixel_path_is_msys_drive(char const *path)
{
    if (path == NULL) {
        return 0;
    }

    return path[0] == '/'
        && path[1] != '\0'
        && path[2] == '/'
        && ((path[1] >= 'A' && path[1] <= 'Z')
            || (path[1] >= 'a' && path[1] <= 'z'))
        && !sixel_path_is_unc(path);
}

static int
sixel_path_is_cygdrive(char const *path)
{
    size_t prefix_len;

    prefix_len = strlen(SIXEL_CYGDRIVE_PREFIX);
    if (path == NULL) {
        return 0;
    }

    if (strncmp(path, SIXEL_CYGDRIVE_PREFIX, prefix_len) != 0) {
        return 0;
    }

    return path[prefix_len] != '\0'
        && path[prefix_len + 1u] == '/'
        && ((path[prefix_len] >= 'A' && path[prefix_len] <= 'Z')
            || (path[prefix_len] >= 'a' && path[prefix_len] <= 'z'));
}

/* These helpers are only referenced by the Cygwin-specific code paths. */
#if defined(__CYGWIN__)
static int
sixel_path_is_drive_letter(char const *path)
{
    if (path == NULL) {
        return 0;
    }

    return ((path[0] >= 'A' && path[0] <= 'Z')
            || (path[0] >= 'a' && path[0] <= 'z'))
        && path[1] == ':'
        && (path[2] == '/' || path[2] == '\\');
}

static size_t
sixel_path_to_cygdrive_size(char const *path)
{
    size_t length;
    size_t extra;

    length = strlen(path);
    extra = strlen(SIXEL_CYGDRIVE_PREFIX) + 2u;
    if (length < 3u) {
        return 0u;
    }

    return length + extra - 3u;
}
#endif
#endif

size_t
sixel_path_to_libc_buffer_size(char const *path)
{
    size_t prefix_len;

    if (path == NULL) {
        return 0u;
    }

#if defined(__CYGWIN__)
    (void)prefix_len;
    if (sixel_path_is_drive_letter(path)) {
        return sixel_path_to_cygdrive_size(path) + 1u;
    }
    return 0u;
#elif defined(_WIN32)
    if (sixel_path_is_msys_drive(path)) {
        return strlen(path) + 1u;
    }
    if (sixel_path_is_cygdrive(path)) {
        prefix_len = strlen(SIXEL_CYGDRIVE_PREFIX);
        return strlen(path) - (prefix_len - 1u) + 1u;
    }
    return 0u;
#else
    (void)prefix_len;
    return 0u;
#endif
}

char const *
sixel_path_to_libc(char const *path,
                   char *buffer,
                   size_t buffer_size)
{
    size_t needed;
    size_t prefix_len;
    size_t remaining;
    char drive;
    char const *rest;
    size_t index;
    size_t out_index;

    if (path == NULL) {
        return NULL;
    }

    needed = sixel_path_to_libc_buffer_size(path);
    if (needed == 0u) {
        return path;
    }
    if (buffer == NULL || buffer_size < needed) {
        return NULL;
    }

#if defined(__CYGWIN__)
    (void)remaining;
    if (!sixel_path_is_drive_letter(path)) {
        return path;
    }

    drive = path[0];
    rest = path + 2u;
    if (*rest == '/' || *rest == '\\') {
        rest++;
    }

    prefix_len = strlen(SIXEL_CYGDRIVE_PREFIX);
    memcpy(buffer, SIXEL_CYGDRIVE_PREFIX, prefix_len);
    buffer[prefix_len] = (char)tolower((unsigned char)drive);
    buffer[prefix_len + 1u] = '/';

    out_index = prefix_len + 2u;
    for (index = 0u; rest[index] != '\0'; index++) {
        if (out_index + 1u >= buffer_size) {
            return NULL;
        }
        buffer[out_index] = rest[index] == '\\' ? '/' : rest[index];
        out_index++;
    }
    buffer[out_index] = '\0';
    return buffer;
#elif defined(_WIN32)
    (void)index;
    (void)out_index;
    if (sixel_path_is_cygdrive(path)) {
        prefix_len = strlen(SIXEL_CYGDRIVE_PREFIX);
        drive = path[prefix_len];
        rest = path + prefix_len + 1u;
        remaining = strlen(rest);
        if (remaining + 2u >= buffer_size) {
            return NULL;
        }
        buffer[0] = drive;
        buffer[1] = ':';
        memcpy(buffer + 2u, rest, remaining + 1u);
        return buffer;
    }
    if (sixel_path_is_msys_drive(path)) {
        /* Use the compat layer to avoid CRT warnings on Windows builds. */
        (void)sixel_compat_strcpy(buffer, buffer_size, path);
        buffer[0] = buffer[1];
        buffer[1] = ':';
        return buffer;
    }
    return path;
#else
    (void)buffer;
    (void)buffer_size;
    (void)prefix_len;
    (void)remaining;
    (void)drive;
    (void)rest;
    (void)index;
    (void)out_index;
    return path;
#endif
}
