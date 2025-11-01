/*
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
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

#ifndef SIXEL_COMPAT_STUB_H
#define SIXEL_COMPAT_STUB_H

#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#if defined(_WIN32) && !defined(_SSIZE_T_DEFINED)
# include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif

#if defined(__GNUC__) || defined(__clang__)
/*
 * +----------------------------------------------------------+
 * |  Format attribute selection matrix                       |
 * +----------------------------------------------------------+
 * |  Platform        | Attribute archetype                   |
 * |------------------+---------------------------------------|
 * |  MinGW targets   | __MINGW_PRINTF_FORMAT                 |
 * |  Other targets   | __printf__                            |
 * +----------------------------------------------------------+
 * We explicitly choose the MinGW specific archetype so that
 * `%zu` and friends are accepted when GCC performs compile
 * time format checks.  Without this mapping, GCC trusts the
 * MSVCRT dialect, immediately flagging `%zu` as an unknown
 * conversion and breaking our build with `-Wformat=2`.
 */
# if defined(__MINGW32__)
#  define SIXEL_PRINTF_ARCHETYPE __MINGW_PRINTF_FORMAT
# else
#  define SIXEL_PRINTF_ARCHETYPE __printf__
# endif
# define SIXEL_ATTRIBUTE_FORMAT(archetype, string_index, first_to_check) \
    __attribute__((__format__(archetype, string_index, first_to_check)))
#else
# define SIXEL_PRINTF_ARCHETYPE __printf__
# define SIXEL_ATTRIBUTE_FORMAT(archetype, string_index, first_to_check)
#endif

/*
 * +------------------------------------------------------------+
 * |  Compatibility helpers for securing libc interactions      |
 * +------------------------------------------------------------+
 * These helpers wrap platform specific secure CRT functions and
 * provide portable fallbacks.  The goal is to make MSVC happy
 * without sprinkling _CRT_SECURE_NO_WARNINGS across the code
 * base while still relying on familiar libc semantics elsewhere.
 */

int sixel_compat_snprintf(char *buffer,
                          size_t buffer_size,
                          const char *format,
                          ...)
    SIXEL_ATTRIBUTE_FORMAT(SIXEL_PRINTF_ARCHETYPE, 3, 4);

int sixel_compat_vsnprintf(char *buffer,
                           size_t buffer_size,
                           const char *format,
                           va_list args)
    SIXEL_ATTRIBUTE_FORMAT(SIXEL_PRINTF_ARCHETYPE, 3, 0);

int sixel_compat_strcpy(char *destination,
                        size_t destination_size,
                        const char *source);

char *sixel_compat_strerror(int error_number,
                            char *buffer,
                            size_t buffer_size);

FILE *sixel_compat_fopen(const char *filename, const char *mode);

const char *sixel_compat_getenv(const char *name);

char *sixel_compat_strtok(char *string,
                          const char *delimiters,
                          char **context);

int sixel_compat_mktemp(char *templ, size_t buffer_size);

int sixel_compat_open(const char *path, int flags, ...);

int sixel_compat_close(int fd);

int sixel_compat_unlink(const char *path);

int sixel_compat_access(const char *path, int mode);

ssize_t sixel_compat_write(int fd, const void *buffer, size_t count);

#endif /* SIXEL_COMPAT_STUB_H */
