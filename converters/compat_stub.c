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

#if !defined(_POSIX_C_SOURCE)
# define _POSIX_C_SOURCE 200809L
#endif

#include "config.h"

/* STDC_HEADERS */
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#if HAVE_STRING_H
# include <string.h>
#endif
#if HAVE_FCNTL_H
# include <fcntl.h>
#endif
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if HAVE_UNISTD_H
# include <unistd.h>
#endif

#if defined(_WIN32)
# include <io.h>
# include <direct.h>
#endif

#if defined(_MSC_VER)
# include <share.h>
#endif

#include "compat_stub.h"

/*
 *  String helpers
 *
 *  They copy textual data with explicit bounds so that we can
 *  rely on the secure CRT without replicating the pattern all
 *  across the project.
 */

SIXEL_COMPAT_API int
sixel_compat_strcpy(char *destination,
                    size_t destination_size,
                    const char *source)
{
    size_t length;

    if (destination == NULL || source == NULL || destination_size == 0) {
        return (-1);
    }

#if defined(_MSC_VER)
    if (strcpy_s(destination, destination_size, source) != 0) {
        return (-1);
    }
    return (int)strlen(destination);
#else
    length = strlen(source);
    if (length >= destination_size) {
        length = destination_size - 1;
    }
    memcpy(destination, source, length);
    destination[length] = '\0';
    return (int)length;
#endif
}


SIXEL_COMPAT_API char *
sixel_compat_strerror(int error_number,
                      char *buffer,
                      size_t buffer_size)
{
#if defined(_MSC_VER)
    errno_t status;
#elif defined(_WIN32)
# if defined(__STDC_LIB_EXT1__)
    errno_t status;
# else
    char *message;
    size_t copy_length;
# endif
#else
# if defined(_GNU_SOURCE)
    char *message;
    size_t copy_length;
# endif
#endif

    if (buffer == NULL || buffer_size == 0) {
        return NULL;
    }

#if defined(_MSC_VER)
    status = strerror_s(buffer, buffer_size, error_number);
    if (status != 0) {
        buffer[0] = '\0';
        return NULL;
    }
    return buffer;
#elif defined(_WIN32)
    /*
     * +----------------------------------------------------+
     * |  Windows family error messages                     |
     * +----------------------------------------------------+
     * |  CRT flavor  |  Routine we can rely on             |
     * |------------- +-------------------------------------|
     * |  Annex K     |  strerror_s()                       |
     * |  Legacy      |  strerror() + manual copy           |
     * +----------------------------------------------------+
     * The secure CRT is present both with MSVC and with
     * clang + UCRT.  When Annex K is unavailable we fall
     * back to strerror() while still clamping the output.
     */
# if defined(__STDC_LIB_EXT1__)
    status = strerror_s(buffer, buffer_size, error_number);
    if (status != 0) {
        buffer[0] = '\0';
        return NULL;
    }
    return buffer;
# else
    message = strerror(error_number);
    if (message == NULL) {
        buffer[0] = '\0';
        return NULL;
    }
    copy_length = buffer_size - 1;
    (void)strncpy(buffer, message, copy_length);
    buffer[buffer_size - 1] = '\0';
    return buffer;
# endif
#else
# if defined(_GNU_SOURCE)
    message = strerror_r(error_number, buffer, buffer_size);
    if (message == NULL) {
        return NULL;
    }
    if (message != buffer) {
        copy_length = buffer_size - 1;
        (void)strncpy(buffer, message, copy_length);
        buffer[buffer_size - 1] = '\0';
    }
    return buffer;
# else
    if (strerror_r(error_number, buffer, buffer_size) != 0) {
        buffer[0] = '\0';
        return NULL;
    }
    return buffer;
# endif
#endif
}


SIXEL_COMPAT_API FILE *
sixel_compat_fopen(const char *filename, const char *mode)
{
    FILE *handle;

    handle = NULL;
    if (filename == NULL || mode == NULL) {
        errno = EINVAL;
        return NULL;
    }

#if defined(_MSC_VER)
    /*
     * Windows refuses to reopen a file for reading when another descriptor
     * still holds an exclusive write handle.  `_fsopen()` lets us request
     * the POSIX-style behaviour where read/write callers can share the same
     * file.  The diagram below sketches the handshake between the writer and
     * reader:
     *
     *   writer (encoder)  ---- creates temp file ---->  `png_temp_path`
     *           |                                         |
     *           |<--- `_fsopen(..., _SH_DENYNO)` --- reader (decoder)
     *
     * Both sides now share the handle without tripping over `_SH_DENYWR`
     * defaults, mirroring how Unix would allow the concurrent access.
     */
    handle = _fsopen(filename, mode, _SH_DENYNO);
#else
    handle = fopen(filename, mode);
#endif

    return handle;
}


SIXEL_COMPAT_API const char *
sixel_compat_getenv(const char *name)
{
#if defined(_MSC_VER)
    static char buffer[32768];
    char *value;
    size_t length;

    value = NULL;
    length = 0;
    if (_dupenv_s(&value, &length, name) != 0) {
        if (value != NULL) {
            free(value);
        }
        return NULL;
    }
    if (value == NULL) {
        return NULL;
    }
    if (length >= sizeof(buffer)) {
        length = sizeof(buffer) - 1;
    }
    memcpy(buffer, value, length);
    buffer[length] = '\0';
    free(value);
    return buffer;
#else
    return getenv(name);
#endif
}


SIXEL_COMPAT_API char *
sixel_compat_strtok(char *string,
                    const char *delimiters,
                    char **context)
{
#if defined(_MSC_VER)
    return strtok_s(string, delimiters, context);
#elif defined(_POSIX_VERSION)
    return strtok_r(string, delimiters, context);
#else
    (void)context;
    return strtok(string, delimiters);
#endif
}


SIXEL_COMPAT_API int
sixel_compat_close(int fd)
{
#if defined(_MSC_VER)
    return _close(fd);
#else
    return close(fd);
#endif
}


SIXEL_COMPAT_API int
sixel_compat_unlink(const char *path)
{
#if defined(_MSC_VER)
    return _unlink(path);
#else
    return unlink(path);
#endif
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
