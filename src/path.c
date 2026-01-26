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

#include "path.h"

#define SIXEL_CYGDRIVE_PREFIX "/cygdrive/"

#if defined(_WIN32) || defined(__CYGWIN__) || defined(__MSYS__)
static int
sixel_path_is_unc(char const *path)
{
    if (path == NULL) {
        return 0;
    }

    return path[0] == '/' && path[1] == '/';
}

static int
sixel_path_parse_drive_letter(char const *path,
                              char *drive,
                              char const **rest)
{
    if (path == NULL) {
        return 0;
    }

    if ((path[0] >= 'A' && path[0] <= 'Z')
        || (path[0] >= 'a' && path[0] <= 'z')) {
        if (path[1] == ':' && (path[2] == '/' || path[2] == '\\')) {
            *drive = path[0];
            *rest = path + 2u;
            return 1;
        }
    }

    return 0;
}

static int
sixel_path_parse_msys_drive(char const *path,
                            char *drive,
                            char const **rest)
{
    if (path == NULL) {
        return 0;
    }

    if (path[0] == '/'
        && path[1] != '\0'
        && path[2] == '/'
        && ((path[1] >= 'A' && path[1] <= 'Z')
            || (path[1] >= 'a' && path[1] <= 'z'))
        && !sixel_path_is_unc(path)) {
        *drive = path[1];
        *rest = path + 2u;
        return 1;
    }

    return 0;
}

static int
sixel_path_parse_cygdrive(char const *path,
                          char *drive,
                          char const **rest)
{
    size_t prefix_len;

    prefix_len = strlen(SIXEL_CYGDRIVE_PREFIX);
    if (path == NULL) {
        return 0;
    }

    if (strncmp(path, SIXEL_CYGDRIVE_PREFIX, prefix_len) != 0) {
        return 0;
    }

    if (path[prefix_len] == '\0' || path[prefix_len + 1u] != '/') {
        return 0;
    }

    if ((path[prefix_len] >= 'A' && path[prefix_len] <= 'Z')
        || (path[prefix_len] >= 'a' && path[prefix_len] <= 'z')) {
        *drive = path[prefix_len];
        *rest = path + prefix_len + 1u;
        return 1;
    }

    return 0;
}

static int
sixel_path_parse_nested_cygdrive(char const *path,
                                 char *drive,
                                 char const **rest)
{
    char msys_drive;
    char const *msys_rest;
    size_t prefix_len;

    /*
     * Some Windows environments pass mixed paths like /c/cygdrive/c/...,
     * so detect and normalize the embedded cygdrive segment.
     */
    if (!sixel_path_parse_msys_drive(path, &msys_drive, &msys_rest)) {
        return 0;
    }

    prefix_len = strlen(SIXEL_CYGDRIVE_PREFIX);
    if (strncmp(msys_rest, SIXEL_CYGDRIVE_PREFIX, prefix_len) != 0) {
        return 0;
    }

    if (msys_rest[prefix_len] == '\0'
        || msys_rest[prefix_len + 1u] != '/') {
        return 0;
    }

    if ((msys_rest[prefix_len] >= 'A' && msys_rest[prefix_len] <= 'Z')
        || (msys_rest[prefix_len] >= 'a' && msys_rest[prefix_len] <= 'z')) {
        *drive = msys_rest[prefix_len];
        *rest = msys_rest + prefix_len + 1u;
        return 1;
    }

    return 0;
}
#endif

#if defined(_WIN32) || defined(__CYGWIN__) || defined(__MSYS__)
static size_t
sixel_path_to_cygdrive_needed(char const *rest)
{
    size_t prefix_len;

    prefix_len = strlen(SIXEL_CYGDRIVE_PREFIX);
    return prefix_len + strlen(rest) + 2u;
}

static size_t
sixel_path_to_msys_needed(char const *rest)
{
    return strlen(rest) + 3u;
}
#endif

size_t
sixel_path_to_libc_buffer_size(char const *path)
{
    char drive;
    char const *rest;

    if (path == NULL) {
        return 0u;
    }

#if defined(__CYGWIN__)
    if (sixel_path_parse_drive_letter(path, &drive, &rest)) {
        return sixel_path_to_cygdrive_needed(rest);
    }
    if (sixel_path_parse_nested_cygdrive(path, &drive, &rest)) {
        return sixel_path_to_cygdrive_needed(rest);
    }
    if (sixel_path_parse_msys_drive(path, &drive, &rest)) {
        return sixel_path_to_cygdrive_needed(rest);
    }
    return 0u;
#elif defined(__MSYS__)
    if (sixel_path_parse_nested_cygdrive(path, &drive, &rest)) {
        return sixel_path_to_msys_needed(rest);
    }
    if (sixel_path_parse_cygdrive(path, &drive, &rest)) {
        return sixel_path_to_msys_needed(rest);
    }
    if (sixel_path_parse_drive_letter(path, &drive, &rest)) {
        return sixel_path_to_msys_needed(rest);
    }
    return 0u;
#elif defined(_WIN32)
    if (sixel_path_parse_drive_letter(path, &drive, &rest)) {
        return 0u;
    }
    if (sixel_path_parse_nested_cygdrive(path, &drive, &rest)) {
        return strlen(rest) + 3u;
    }
    if (sixel_path_parse_cygdrive(path, &drive, &rest)) {
        return strlen(rest) + 3u;
    }
    if (sixel_path_parse_msys_drive(path, &drive, &rest)) {
        return strlen(rest) + 3u;
    }
    return 0u;
#else
    (void)drive;
    (void)rest;
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
    if (sixel_path_parse_cygdrive(path, &drive, &rest)) {
        return path;
    }
    if (sixel_path_parse_nested_cygdrive(path, &drive, &rest)
        || sixel_path_parse_msys_drive(path, &drive, &rest)
        || sixel_path_parse_drive_letter(path, &drive, &rest)) {
        prefix_len = strlen(SIXEL_CYGDRIVE_PREFIX);
        memcpy(buffer, SIXEL_CYGDRIVE_PREFIX, prefix_len);
        buffer[prefix_len] = (char)tolower((unsigned char)drive);
        out_index = prefix_len + 1u;
        for (index = 0u; rest[index] != '\0'; index++) {
            if (out_index + 1u >= buffer_size) {
                return NULL;
            }
            buffer[out_index] = rest[index] == '\\' ? '/' : rest[index];
            out_index++;
        }
        buffer[out_index] = '\0';
        return buffer;
    }
    return path;
#elif defined(__MSYS__)
    (void)prefix_len;
    if (sixel_path_parse_msys_drive(path, &drive, &rest)) {
        return path;
    }
    if (sixel_path_parse_nested_cygdrive(path, &drive, &rest)
        || sixel_path_parse_cygdrive(path, &drive, &rest)
        || sixel_path_parse_drive_letter(path, &drive, &rest)) {
        buffer[0] = '/';
        buffer[1] = (char)tolower((unsigned char)drive);
        out_index = 2u;
        for (index = 0u; rest[index] != '\0'; index++) {
            if (out_index + 1u >= buffer_size) {
                return NULL;
            }
            buffer[out_index] = rest[index] == '\\' ? '/' : rest[index];
            out_index++;
        }
        buffer[out_index] = '\0';
        return buffer;
    }
    return path;
#elif defined(_WIN32)
    (void)prefix_len;
    if (sixel_path_parse_drive_letter(path, &drive, &rest)) {
        return path;
    }
    if (sixel_path_parse_nested_cygdrive(path, &drive, &rest)
        || sixel_path_parse_cygdrive(path, &drive, &rest)
        || sixel_path_parse_msys_drive(path, &drive, &rest)) {
        buffer[0] = drive;
        buffer[1] = ':';
        out_index = 2u;
        for (index = 0u; rest[index] != '\0'; index++) {
            if (out_index + 1u >= buffer_size) {
                return NULL;
            }
            buffer[out_index] = rest[index] == '\\' ? '/' : rest[index];
            out_index++;
        }
        buffer[out_index] = '\0';
        return buffer;
    }
    return path;
#else
    (void)buffer;
    (void)buffer_size;
    (void)prefix_len;
    (void)drive;
    (void)rest;
    (void)index;
    (void)out_index;
    return path;
#endif
}
