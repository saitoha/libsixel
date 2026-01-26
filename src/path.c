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

#if HAVE_EMSCRIPTEN_H
#include <emscripten.h>
#endif  /* HAVE_EMSCRIPTEN_H */

#if defined(__COSMOPOLITAN__)
#include <cosmo.h>
#endif  /* __COSMOPOLITAN__ */

#include "path.h"

#define SIXEL_CYGDRIVE_PREFIX "/cygdrive/"

#if defined(_WIN32) || defined(__CYGWIN__) || defined(__MSYS__) \
    || defined(__EMSCRIPTEN__) || defined(__COSMOPOLITAN__)
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

#if defined(__COSMOPOLITAN__)
static int
sixel_path_cosmo_is_windows(void)
{
    /*
     * Cosmopolitan exposes the runtime OS via IsWindows(). Keep this helper
     * so we can guard conversions without scattering the API across branches.
     */
    return IsWindows() ? 1 : 0;
}
#endif

#if defined(__EMSCRIPTEN__)
static int
sixel_path_emscripten_rawfs_enabled(void)
{
    char const *setting;

    /*
     * Decide whether /c/... and /cygdrive/c/... paths should be treated as
     * Windows-native. The logic is intentionally defensive:
     *
     * 1. If emscripten_get_compiler_setting is missing, assume enabled.
     * 2. If the returned string is empty or NULL, assume enabled.
     * 3. Treat "0"/"false" (case-insensitive) as disabled, everything else
     *    as enabled.
     */
#if !HAVE_EMSCRIPTEN_GET_COMPILER_SETTING
    return 1;
#else
    setting = (char const *)emscripten_get_compiler_setting("NODERAWFS");
    if (setting == NULL || setting[0] == '\0') {
        return 1;
    }
    if (setting[0] == '0'
        || setting[0] == 'f'
        || setting[0] == 'F') {
        return 0;
    }
    return 1;
#endif
}
#endif

#if defined(__MSYS__)
/*
 * Keep the helper scoped to the platforms that consume it so
 * -Wunused-function does not trigger in Windows-only builds.
 */
static size_t
sixel_path_to_msys_needed(char const *rest)
{
    return strlen(rest) + 3u;
}
#elif defined(__CYGWIN__)
/*
 * Keep the helper scoped to the platforms that consume it so
 * -Wunused-function does not trigger in Windows-only builds.
 */
static size_t
sixel_path_to_cygdrive_needed(char const *rest)
{
    size_t prefix_len;

    prefix_len = strlen(SIXEL_CYGDRIVE_PREFIX);
    return prefix_len + strlen(rest) + 2u;
}
#endif

size_t
sixel_path_to_libc_buffer_size(char const *path)
{
    char drive;
    char const *rest;
#if defined(__EMSCRIPTEN__) || defined(__COSMOPOLITAN__)
    int rawfs_enabled;
#endif

    if (path == NULL) {
        return 0u;
    }

#if defined(__MSYS__)
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
#elif defined(__CYGWIN__)
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
#elif defined(__COSMOPOLITAN__)
    if (!sixel_path_cosmo_is_windows()) {
        return 0u;
    }
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
#elif defined(__EMSCRIPTEN__)
    rawfs_enabled = sixel_path_emscripten_rawfs_enabled();
    if (!rawfs_enabled) {
        return 0u;
    }
    if (sixel_path_parse_msys_drive(path, &drive, &rest)) {
        return strlen(rest) + 3u;
    }
    if (sixel_path_parse_cygdrive(path, &drive, &rest)) {
        return strlen(rest) + 3u;
    }
    if (sixel_path_parse_nested_cygdrive(path, &drive, &rest)) {
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
#if defined(__EMSCRIPTEN__) || defined(__COSMOPOLITAN__)
    int rawfs_enabled;
#endif

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

#if defined(__MSYS__)
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
#elif defined(__CYGWIN__)
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
#elif defined(__COSMOPOLITAN__)
    (void)prefix_len;
    if (!sixel_path_cosmo_is_windows()) {
        return path;
    }
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
#elif defined(__EMSCRIPTEN__)
    (void)prefix_len;
    rawfs_enabled = sixel_path_emscripten_rawfs_enabled();
    if (!rawfs_enabled) {
        return path;
    }
    if (sixel_path_parse_drive_letter(path, &drive, &rest)) {
        return path;
    }
    if (sixel_path_parse_msys_drive(path, &drive, &rest)
        || sixel_path_parse_cygdrive(path, &drive, &rest)
        || sixel_path_parse_nested_cygdrive(path, &drive, &rest)) {
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
#if defined(__EMSCRIPTEN__) || defined(__COSMOPOLITAN__)
    (void)rawfs_enabled;
#endif
    return path;
#endif
}
