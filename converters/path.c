/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
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
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif  /* HAVE_STDLIB_H */
#if HAVE_STDIO_H
#include <stdio.h>
#endif  /* HAVE_STDIO_H */
#if HAVE_UNISTD_H
#include <unistd.h>
#endif  /* HAVE_UNISTD_H */
#if HAVE_EMSCRIPTEN_H
#include <emscripten.h>
#endif  /* HAVE_EMSCRIPTEN_H */

#if defined(__COSMOPOLITAN__)
#include <cosmo.h>
#endif  /* __COSMOPOLITAN__ */

#if defined(__CYGWIN__) || defined(__MSYS__)
#include <sys/cygwin.h>
#endif  /* __CYGWIN__ || __MSYS__ */

#include "path.h"

#define IMG2SIXEL_CYGDRIVE_PREFIX "/cygdrive/"

#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MSYS__)
#define IMG2SIXEL_PATH_USE_CYGPATH 1
#elif defined(__COSMOPOLITAN__)
#define IMG2SIXEL_PATH_USE_CYGPATH 1
#endif

#if defined(IMG2SIXEL_PATH_USE_CYGPATH)
#if defined(_MSC_VER)
#define img2sixel_path_popen _popen
#define img2sixel_path_pclose _pclose
#else
#define img2sixel_path_popen popen
#define img2sixel_path_pclose pclose
#endif
#endif

#if defined(_WIN32) || defined(__CYGWIN__) || defined(__MSYS__) \
    || defined(__EMSCRIPTEN__) || defined(__COSMOPOLITAN__)
static int
img2sixel_path_is_unc(char const *path)
{
    if (path == NULL) {
        return 0;
    }

    return path[0] == '/' && path[1] == '/';
}

static int
img2sixel_path_parse_drive_letter(char const *path,
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
img2sixel_path_parse_msys_drive(char const *path,
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
        && !img2sixel_path_is_unc(path)) {
        *drive = path[1];
        *rest = path + 2u;
        return 1;
    }

    return 0;
}

static int
img2sixel_path_parse_cygdrive(char const *path,
                              char *drive,
                              char const **rest)
{
    size_t prefix_len;

    prefix_len = strlen(IMG2SIXEL_CYGDRIVE_PREFIX);
    if (path == NULL) {
        return 0;
    }

    if (strncmp(path, IMG2SIXEL_CYGDRIVE_PREFIX, prefix_len) != 0) {
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
img2sixel_path_parse_nested_cygdrive(char const *path,
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
    if (!img2sixel_path_parse_msys_drive(path, &msys_drive, &msys_rest)) {
        return 0;
    }

    prefix_len = strlen(IMG2SIXEL_CYGDRIVE_PREFIX);
    if (strncmp(msys_rest, IMG2SIXEL_CYGDRIVE_PREFIX, prefix_len) != 0) {
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

#if defined(__CYGWIN__) || defined(__MSYS__)
/*
 * cygwin_conv_path() is the authoritative way to translate Windows-style
 * paths into POSIX form for the Cygwin/MSYS runtimes. We only use it to
 * convert to POSIX because libc on these platforms expects POSIX paths.
 */
static int
img2sixel_path_cygwin_conv_target(char const *path)
{
    static char const *clipboard_prefix = "clipboard:";
    size_t prefix_len;

    prefix_len = strlen(clipboard_prefix);
    if (path == NULL) {
        return 0;
    }

    if (path[0] == '\0') {
        return 0;
    }

    /*
     * Skip stdin/stdout sentinels and pseudo targets that are interpreted
     * by the CLI layer instead of the filesystem.
     */
    if (strcmp(path, "-") == 0) {
        return 0;
    }

    if (strncmp(path, clipboard_prefix, prefix_len) == 0) {
        return 0;
    }

    return 1;
}

static size_t
img2sixel_path_cygwin_conv_needed(char const *path)
{
    ssize_t needed;

    needed = 0;
    if (path == NULL) {
        return 0u;
    }
    if (!img2sixel_path_cygwin_conv_target(path)) {
        return 0u;
    }

    needed = cygwin_conv_path(CCP_WIN_A_TO_POSIX, path, NULL, 0);
    if (needed <= 0) {
        return 0u;
    }

    return (size_t)needed;
}

static char const *
img2sixel_path_cygwin_conv_to_posix(char const *path,
                                    char *buffer,
                                    size_t buffer_size)
{
    size_t needed;
    int result;

    needed = 0u;
    result = 0;

    if (path == NULL) {
        return NULL;
    }
    if (!img2sixel_path_cygwin_conv_target(path)) {
        return NULL;
    }

    needed = img2sixel_path_cygwin_conv_needed(path);
    if (needed == 0u) {
        return NULL;
    }
    if (buffer == NULL || buffer_size < needed) {
        return NULL;
    }

    result = cygwin_conv_path(CCP_WIN_A_TO_POSIX, path, buffer, buffer_size);
    if (result != 0) {
        return NULL;
    }

    return buffer;
}
#endif

#if defined(IMG2SIXEL_PATH_USE_CYGPATH)
/*
 * cygpath -wa provides the most accurate conversion when a POSIX-like path
 * reaches native Windows runtimes such as MinGW/MSVC. The helper executes
 * the tool in a small shell pipeline and captures the output.
 */
static int
img2sixel_path_cygpath_target(char const *path)
{
    if (path == NULL) {
        return 0;
    }

    if (path[0] == '\0') {
        return 0;
    }

    /*
     * Use cygpath only for clear POSIX-like paths so native inputs and
     * sentinels remain untouched. This keeps conversions scoped to paths
     * expected to resolve via MSYS/Cygwin mount logic.
     */
    return path[0] == '/' || path[0] == '~';
}

static size_t
img2sixel_path_shell_quote_needed(char const *path)
{
    size_t needed;
    size_t index;

    needed = 2u;
    if (path == NULL) {
        return 0u;
    }

    for (index = 0u; path[index] != '\0'; index++) {
        if (path[index] == '\'') {
            needed += 4u;
        } else {
            needed += 1u;
        }
    }

    return needed;
}

static int
img2sixel_path_build_cygpath_command(char const *path,
                                     char *buffer,
                                     size_t buffer_size)
{
    static char const *prefix = "cygpath -wa -- ";
    size_t prefix_len;
    size_t quoted_len;
    size_t index;
    size_t out_index;

    prefix_len = 0u;
    quoted_len = 0u;
    index = 0u;
    out_index = 0u;

    if (path == NULL || buffer == NULL) {
        return 0;
    }

    prefix_len = strlen(prefix);
    quoted_len = img2sixel_path_shell_quote_needed(path);
    if (quoted_len == 0u) {
        return 0;
    }

    if (buffer_size <= prefix_len + quoted_len) {
        return 0;
    }

    memcpy(buffer, prefix, prefix_len);
    out_index = prefix_len;
    buffer[out_index] = '\'';
    out_index++;
    for (index = 0u; path[index] != '\0'; index++) {
        if (path[index] == '\'') {
            buffer[out_index] = '\'';
            buffer[out_index + 1u] = '\\';
            buffer[out_index + 2u] = '\'';
            buffer[out_index + 3u] = '\'';
            out_index += 4u;
        } else {
            buffer[out_index] = path[index];
            out_index++;
        }
    }
    buffer[out_index] = '\'';
    out_index++;
    buffer[out_index] = '\0';

    return 1;
}

static char *
img2sixel_path_read_command_output(FILE *pipe_handle)
{
    char *buffer;
    char *next;
    size_t length;
    size_t capacity;
    int ch;

    buffer = NULL;
    next = NULL;
    length = 0u;
    capacity = 128u;
    ch = 0;

    if (pipe_handle == NULL) {
        return NULL;
    }

    buffer = (char *)malloc(capacity);
    if (buffer == NULL) {
        return NULL;
    }

    while ((ch = fgetc(pipe_handle)) != EOF) {
        if (length + 1u >= capacity) {
            capacity *= 2u;
            next = (char *)realloc(buffer, capacity);
            if (next == NULL) {
                free(buffer);
                return NULL;
            }
            buffer = next;
        }
        buffer[length] = (char)ch;
        length++;
    }

    if (ferror(pipe_handle)) {
        free(buffer);
        return NULL;
    }

    while (length > 0u) {
        if (buffer[length - 1u] != '\n'
            && buffer[length - 1u] != '\r') {
            break;
        }
        length--;
    }

    if (length == 0u) {
        free(buffer);
        return NULL;
    }

    buffer[length] = '\0';
    return buffer;
}

static char *
img2sixel_path_cygpath_convert(char const *path)
{
    char *command;
    char *result;
    FILE *pipe_handle;
    size_t needed;
    int status;

    command = NULL;
    result = NULL;
    pipe_handle = NULL;
    needed = 0u;
    status = 0;

    if (path == NULL) {
        return NULL;
    }
    if (!img2sixel_path_cygpath_target(path)) {
        return NULL;
    }

    needed = strlen(path) + img2sixel_path_shell_quote_needed(path) + 32u;
    command = (char *)malloc(needed);
    if (command == NULL) {
        return NULL;
    }

    if (!img2sixel_path_build_cygpath_command(path, command, needed)) {
        free(command);
        return NULL;
    }

    pipe_handle = img2sixel_path_popen(command, "r");
    if (pipe_handle == NULL) {
        free(command);
        return NULL;
    }

    result = img2sixel_path_read_command_output(pipe_handle);
    status = img2sixel_path_pclose(pipe_handle);
    (void)status;
    free(command);

    return result;
}

static size_t
img2sixel_path_cygpath_needed(char const *path)
{
    char *converted;
    size_t length;

    converted = NULL;
    length = 0u;

    if (!img2sixel_path_cygpath_target(path)) {
        return 0u;
    }

    converted = img2sixel_path_cygpath_convert(path);
    if (converted == NULL) {
        return 0u;
    }

    length = strlen(converted) + 1u;
    free(converted);
    return length;
}
#endif

#if defined(__COSMOPOLITAN__)
static int
img2sixel_path_cosmo_is_windows(void)
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
img2sixel_path_emscripten_rawfs_enabled(void)
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
# if !HAVE_EMSCRIPTEN_GET_COMPILER_SETTING
    return 1;
# else
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
# endif
}
#endif

#if defined(__MSYS__)
/*
 * Keep the helper scoped to the platforms that consume it so
 * -Wunused-function does not trigger in Windows-only builds.
 */
static size_t
img2sixel_path_to_msys_needed(char const *rest)
{
    return strlen(rest) + 3u;
}
#elif defined(__CYGWIN__)
/*
 * Keep the helper scoped to the platforms that consume it so
 * -Wunused-function does not trigger in Windows-only builds.
 */
static size_t
img2sixel_path_to_cygdrive_needed(char const *rest)
{
    size_t prefix_len;

    prefix_len = strlen(IMG2SIXEL_CYGDRIVE_PREFIX);
    return prefix_len + strlen(rest) + 2u;
}
#endif

size_t
img2sixel_path_to_libc_buffer_size(char const *path)
{
    char drive;
    char const *rest;
#if defined(__EMSCRIPTEN__)
    int rawfs_enabled;
#endif

    if (path == NULL) {
        return 0u;
    }

#if defined(__MSYS__)
    /*
     * Priority order for Windows-hosted environments:
     * 1. Use cygwin_conv_path when available (authoritative).
     * 2. Fall back to explicit drive/cygdrive parsing.
     */
    {
        size_t cygwin_needed;

        cygwin_needed = img2sixel_path_cygwin_conv_needed(path);
        if (cygwin_needed > 0u) {
            return cygwin_needed;
        }
    }
    if (img2sixel_path_parse_nested_cygdrive(path, &drive, &rest)) {
        return img2sixel_path_to_msys_needed(rest);
    }
    if (img2sixel_path_parse_cygdrive(path, &drive, &rest)) {
        return img2sixel_path_to_msys_needed(rest);
    }
    if (img2sixel_path_parse_drive_letter(path, &drive, &rest)) {
        return img2sixel_path_to_msys_needed(rest);
    }
    return 0u;
#elif defined(__CYGWIN__)
    /*
     * Priority order for Windows-hosted environments:
     * 1. Use cygwin_conv_path when available (authoritative).
     * 2. Fall back to explicit drive/cygdrive parsing.
     */
    {
        size_t cygwin_needed;

        cygwin_needed = img2sixel_path_cygwin_conv_needed(path);
        if (cygwin_needed > 0u) {
            return cygwin_needed;
        }
    }
    if (img2sixel_path_parse_drive_letter(path, &drive, &rest)) {
        return img2sixel_path_to_cygdrive_needed(rest);
    }
    if (img2sixel_path_parse_nested_cygdrive(path, &drive, &rest)) {
        return img2sixel_path_to_cygdrive_needed(rest);
    }
    if (img2sixel_path_parse_msys_drive(path, &drive, &rest)) {
        return img2sixel_path_to_cygdrive_needed(rest);
    }
    return 0u;
#elif defined(_WIN32)
    /*
     * Priority order for Windows-hosted environments:
     * 1. Use cygpath -wa if available on PATH.
     * 2. Fall back to explicit drive/cygdrive parsing.
     */
# if defined(IMG2SIXEL_PATH_USE_CYGPATH)
    {
        size_t cygpath_needed;

        cygpath_needed = img2sixel_path_cygpath_needed(path);
        if (cygpath_needed > 0u) {
            return cygpath_needed;
        }
    }
# endif
    if (img2sixel_path_parse_drive_letter(path, &drive, &rest)) {
        return 0u;
    }
    if (img2sixel_path_parse_nested_cygdrive(path, &drive, &rest)) {
        return strlen(rest) + 3u;
    }
    if (img2sixel_path_parse_cygdrive(path, &drive, &rest)) {
        return strlen(rest) + 3u;
    }
    if (img2sixel_path_parse_msys_drive(path, &drive, &rest)) {
        return strlen(rest) + 3u;
    }
    return 0u;
#elif defined(__COSMOPOLITAN__)
    if (!img2sixel_path_cosmo_is_windows()) {
        return 0u;
    }
    /*
     * Priority order for Windows-hosted environments:
     * 1. Use cygpath -wa if available on PATH.
     * 2. Fall back to explicit drive/cygdrive parsing.
     */
# if defined(IMG2SIXEL_PATH_USE_CYGPATH)
    {
        size_t cygpath_needed;

        cygpath_needed = img2sixel_path_cygpath_needed(path);
        if (cygpath_needed > 0u) {
            return cygpath_needed;
        }
    }
# endif
    if (img2sixel_path_parse_drive_letter(path, &drive, &rest)) {
        return 0u;
    }
    if (img2sixel_path_parse_nested_cygdrive(path, &drive, &rest)) {
        return strlen(rest) + 3u;
    }
    if (img2sixel_path_parse_cygdrive(path, &drive, &rest)) {
        return strlen(rest) + 3u;
    }
    if (img2sixel_path_parse_msys_drive(path, &drive, &rest)) {
        return strlen(rest) + 3u;
    }
    return 0u;
#elif defined(__EMSCRIPTEN__)
    rawfs_enabled = img2sixel_path_emscripten_rawfs_enabled();
    if (!rawfs_enabled) {
        return 0u;
    }
    if (img2sixel_path_parse_msys_drive(path, &drive, &rest)) {
        return strlen(rest) + 3u;
    }
    if (img2sixel_path_parse_cygdrive(path, &drive, &rest)) {
        return strlen(rest) + 3u;
    }
    if (img2sixel_path_parse_nested_cygdrive(path, &drive, &rest)) {
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
img2sixel_path_to_libc(char const *path,
                       char *buffer,
                       size_t buffer_size)
{
    size_t needed;
    size_t prefix_len;
    char drive;
    char const *rest;
    size_t index;
    size_t out_index;
#if defined(__EMSCRIPTEN__)
    int rawfs_enabled;
#endif

    if (path == NULL) {
        return NULL;
    }

    needed = img2sixel_path_to_libc_buffer_size(path);
    if (needed == 0u) {
        return path;
    }
    if (buffer == NULL || buffer_size < needed) {
        return NULL;
    }

#if defined(__MSYS__)
    (void)prefix_len;
    {
        char const *converted;

        converted = img2sixel_path_cygwin_conv_to_posix(path,
                                                        buffer,
                                                        buffer_size);
        if (converted != NULL) {
            return converted;
        }
    }
    if (img2sixel_path_parse_msys_drive(path, &drive, &rest)) {
        return path;
    }
    if (img2sixel_path_parse_nested_cygdrive(path, &drive, &rest)
        || img2sixel_path_parse_cygdrive(path, &drive, &rest)
        || img2sixel_path_parse_drive_letter(path, &drive, &rest)) {
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
    {
        char const *converted;

        converted = img2sixel_path_cygwin_conv_to_posix(path,
                                                        buffer,
                                                        buffer_size);
        if (converted != NULL) {
            return converted;
        }
    }
    if (img2sixel_path_parse_cygdrive(path, &drive, &rest)) {
        return path;
    }
    if (img2sixel_path_parse_nested_cygdrive(path, &drive, &rest)
        || img2sixel_path_parse_msys_drive(path, &drive, &rest)
        || img2sixel_path_parse_drive_letter(path, &drive, &rest)) {
        prefix_len = strlen(IMG2SIXEL_CYGDRIVE_PREFIX);
        memcpy(buffer, IMG2SIXEL_CYGDRIVE_PREFIX, prefix_len);
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
# if defined(IMG2SIXEL_PATH_USE_CYGPATH)
    {
        char *converted;
        size_t length;

        converted = img2sixel_path_cygpath_convert(path);
        if (converted != NULL) {
            length = strlen(converted) + 1u;
            if (length > buffer_size) {
                free(converted);
                return NULL;
            }
            memcpy(buffer, converted, length);
            free(converted);
            return buffer;
        }
    }
# endif
    if (img2sixel_path_parse_drive_letter(path, &drive, &rest)) {
        return path;
    }
    if (img2sixel_path_parse_nested_cygdrive(path, &drive, &rest)
        || img2sixel_path_parse_cygdrive(path, &drive, &rest)
        || img2sixel_path_parse_msys_drive(path, &drive, &rest)) {
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
    if (!img2sixel_path_cosmo_is_windows()) {
        return path;
    }
# if defined(IMG2SIXEL_PATH_USE_CYGPATH)
    {
        char *converted;
        size_t length;

        converted = img2sixel_path_cygpath_convert(path);
        if (converted != NULL) {
            length = strlen(converted) + 1u;
            if (length > buffer_size) {
                free(converted);
                return NULL;
            }
            memcpy(buffer, converted, length);
            free(converted);
            return buffer;
        }
    }
# endif
    if (img2sixel_path_parse_drive_letter(path, &drive, &rest)) {
        return path;
    }
    if (img2sixel_path_parse_nested_cygdrive(path, &drive, &rest)
        || img2sixel_path_parse_cygdrive(path, &drive, &rest)
        || img2sixel_path_parse_msys_drive(path, &drive, &rest)) {
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
    rawfs_enabled = img2sixel_path_emscripten_rawfs_enabled();
    if (!rawfs_enabled) {
        return path;
    }
    if (img2sixel_path_parse_drive_letter(path, &drive, &rest)) {
        return path;
    }
    if (img2sixel_path_parse_msys_drive(path, &drive, &rest)
        || img2sixel_path_parse_cygdrive(path, &drive, &rest)
        || img2sixel_path_parse_nested_cygdrive(path, &drive, &rest)) {
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
# if defined(__EMSCRIPTEN__)
    (void)rawfs_enabled;
# endif
    return path;
#endif
}
