/*
 * SPDX-License-Identifier: MIT
 *
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

#if !defined(_DEFAULT_SOURCE)
/*
 * glibc hides gettimeofday() behind _DEFAULT_SOURCE when POSIX feature
 * macros tighten the exposed surface.  Keep the default extensions on so
 * legacy time helpers stay declared even after we request POSIX 2008 APIs.
 */
# define _DEFAULT_SOURCE 1
#endif

#if !defined(_POSIX_C_SOURCE)
# define _POSIX_C_SOURCE 200809L
#endif

#if defined(_WIN32) && !defined(__STDC_WANT_SECURE_LIB__)
# define __STDC_WANT_SECURE_LIB__ 1
#endif

#if defined(_WIN32) && !defined(_CRT_DECLARE_NONSTDC_NAMES)
# define _CRT_DECLARE_NONSTDC_NAMES 1
#endif

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

/* STDC_HEADERS */
#include <stdio.h>
#include <stdlib.h>

#if HAVE_ERRNO_H
# include <errno.h>
#endif
#if HAVE_STDARG_H
# include <stdarg.h>
#endif
#if HAVE_LIMITS_H
# include <limits.h>
#endif
#if HAVE_TIME_H
# include <time.h>
#endif
#if HAVE_STRING_H
# include <string.h>
#endif
#if HAVE_STRINGS_H
# include <strings.h>
#endif
#if HAVE_FCNTL_H
# include <fcntl.h>
#endif
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#if defined(_WIN32)
# if !defined(UNICODE)
#  define UNICODE
# endif
# if !defined(_UNICODE)
#  define _UNICODE
# endif
# if !defined(WIN32_LEAN_AND_MEAN)
#  define WIN32_LEAN_AND_MEAN
# endif
# include <windows.h>
# if 0
# if defined(_MSC_VER)
#  include <winsock2.h>  /* for struct timeval */
# endif  /* defined(_MSC_VER) */
# endif
# if defined(_MSC_VER)
typedef struct timeval {
  long tv_sec;
  long tv_usec;
} TIMEVAL, *PTIMEVAL, *LPTIMEVAL;
# endif
#endif

/*
 * Some libcs (notably glibc when feature macros are trimmed by the build
 * flags) expose `realpath()` at link time but hide the prototype.  Declare it
 * ourselves when configure detected the symbol so that we do not rely on
 * implicit declarations.
 */
#if defined(HAVE_REALPATH)
char *realpath(const char *path, char *resolved_path);
#endif

#if 0
/*
 * FreeBSD trims `gettimeofday()` from the public surface when strict POSIX
 * feature macros are enabled even though the symbol is still provided.
 * Configure probes the public prototype to record which timezone argument
 * variant the system exposes so we can mirror the declaration without
 * guessing per-platform patterns.
 */
#if defined(HAVE_GETTIMEOFDAY)
# if defined(HAVE_GETTIMEOFDAY_TZ_VOID) && !defined(__OpenBSD__)
int gettimeofday(struct timeval *tv, void *tz);
# elif defined(HAVE_GETTIMEOFDAY_TZ_TIMEZONE)
struct timezone;
int gettimeofday(struct timeval *tv, struct timezone *tz);
# endif
#endif
#endif

#if defined(__FreeBSD__) || defined(__DragonFly__)
struct timezone;
int gettimeofday(struct timeval *tv, struct timezone *tz);
#endif

#if defined(_WIN32)
# include <io.h>
# include <direct.h>
#endif

#if defined(_MSC_VER)
# include <share.h>
#endif

#include "compat_stub.h"
#include "path.h"

/*
 * MSVC deprecates POSIX getcwd(). Use the secure CRT spelling when
 * available to silence warnings without changing behavior elsewhere.
 */
#if defined(_MSC_VER)
# define sixel_compat_getcwd _getcwd
#else
# define sixel_compat_getcwd getcwd
#endif

#if !defined(_CRTIMP)
# define _CRTIMP
#endif

#if defined(_WIN32) && (HAVE__DUPENV_S || defined(_MSC_VER))
/*
 * Some Windows SDKs require feature macros to expose `_dupenv_s()`.  The
 * declaration below acts as a safety net when headers remain silent even
 * after we request the secure CRT extensions.  Match the platform
 * attributes so that MinGW's dllimport decoration stays consistent.
 */
_CRTIMP errno_t __cdecl _dupenv_s(char **buffer,
                                  size_t *length,
                                  const char *name);
#endif

#if HAVE__SETMODE && (defined(_WIN32) || defined(__CYGWIN__) || \
    defined(__MINGW32__) || defined(__MSYS__))
/*
 * Some MinGW/MSYS headers hide `_setmode()` when POSIX feature macros
 * are enabled.  Declare it ourselves to keep the prototype visible
 * while still using the system implementation.  Restrict the
 * declaration to Windows/Unix-compat runtimes that provide the CRT
 * calling convention so we avoid leaking the symbol onto pure POSIX
 * builds.  The signature matches the public CRT contract.
 */
_CRTIMP int __cdecl _setmode(int fd, int mode);
# define SIXEL_HAVE_WIN_SETMODE 1
#endif

#if defined(__APPLE__) && defined(__clang__)
/*
 * +------------------------------------------------------------+
 * |  Fortified stdio vs. dynamic format strings                |
 * +------------------+-----------------------------------------+
 * | vsnprintf macro  |  __builtin___vsnprintf_chk              |
 * |------------------+-----------------------------------------|
 * | our wrapper call |           dynamic "format"              |
 * +------------------+-----------------------------------------+
 * Clang promotes vsnprintf() to a fortified builtin on Darwin.
 * The builtin insists on string literals so that it can audit
 * the format at compile time.  Our wrappers already perform the
 * format audit at the call site through the printf attribute, so
 * we explicitly opt out from the fortified macro and reach the
 * real function symbol instead.
 */
# undef vsnprintf
#endif

#if !defined(va_copy)
# define va_copy(dest, src) ((dest) = (src))
#endif

/*
 *  Formatting helpers
 *
 *  We provide a thin abstraction over the secure CRT whenever
 *  it is available.  On other platforms we keep using the
 *  classic interfaces.  All functions return the number of
 *  characters that would have been written, mirroring snprintf().
 */

SIXEL_COMPAT_API int
sixel_compat_vsnprintf(char *buffer,
                       size_t buffer_size,
                       const char *format,
                       va_list args)
{
    int written;
    va_list args_copy;
#if defined(_MSC_VER)
    int msvc_result;
#endif

    written = (-1);
    if (format == NULL) {
        return written;
    }

#if defined(_MSC_VER)
    /*
     * +------------------------------------------------------------+
     * |  Dual-pass flow for the MSVC secure CRT                    |
     * +-------------------+----------------------------------------+
     * | 1. length probe   | _vscprintf() walks the argument list.  |
     * | 2. final write    | _vsnprintf_s() consumes the original   |
     * |                   | argument list while clamping writes.   |
     * +-------------------+----------------------------------------+
     * The secure CRT insists on receiving the untouched argument
     * list so we do not clone it for the second phase.  MinGW uses
     * a distinct runtime and follows the branch below instead.
     */
    va_copy(args_copy, args);
    written = _vscprintf(format, args_copy);
    va_end(args_copy);
    if (buffer_size > 0 && written >= 0) {
        msvc_result = _vsnprintf_s(buffer,
                                   buffer_size,
                                   _TRUNCATE,
                                   format,
                                   args);
        if (msvc_result < 0) {
            written = (-1);
        }
    }
#elif defined(_WIN32)
    va_copy(args_copy, args);
    written = _vscprintf(format, args_copy);
    va_end(args_copy);
    if (buffer_size > 0) {
        /*
         * +-------------------+-------------------------------+
         * | phase             | work performed                |
         * +-------------------+-------------------------------+
         * | 1. length probe   | _vscprintf() counts the bytes |
         * | 2. final write    | vsnprintf() copies to buffer  |
         * +-------------------+-------------------------------+
         * MinGW inherits the legacy MSVCRT behaviour where the
         * "NULL,0" probe fails.  The two-step dance above keeps
         * the interfaces happy on both Windows and POSIX.
         */
        va_copy(args_copy, args);
        (void)vsnprintf(buffer, buffer_size, format, args_copy);
        va_end(args_copy);
    }
#else
    va_copy(args_copy, args);
    written = vsnprintf(NULL, (size_t)0, format, args_copy);
    va_end(args_copy);
    if (buffer_size > 0) {
        va_copy(args_copy, args);
        (void)vsnprintf(buffer, buffer_size, format, args_copy);
        va_end(args_copy);
    }
#endif

    return written;
}



/*
 * Case-insensitive string comparison used by runtime configuration parsing.
 * The helper mirrors strcmp() semantics and tolerates NULL pairs so callers
 * can reuse it in defensive paths without extra guards.
 */
SIXEL_COMPAT_API int
sixel_compat_strcasecmp(char const *lhs, char const *rhs)
{
    int result;

    result = 0;

    if (lhs == NULL || rhs == NULL) {
        if (lhs == rhs) {
            return 0;
        }
        return (lhs == NULL) ? -1 : 1;
    }

#if defined(_WIN32)
    result = _stricmp(lhs, rhs);
#else
    result = strcasecmp(lhs, rhs);
#endif

    return result;
}


SIXEL_COMPAT_API int
sixel_compat_snprintf(char *buffer,
                      size_t buffer_size,
                      const char *format,
                      ...)
{
    int written;
    va_list args;

    written = (-1);
    va_start(args, format);
    written = sixel_compat_vsnprintf(buffer,
                                     buffer_size,
                                     format,
                                     args);
    va_end(args);

    return written;
}


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
#if !defined(_MSC_VER)
    size_t length;
#endif

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


/*
 * Portable byte move helper
 *
 * Some platforms hide memmove() behind feature macros.  Keep the call
 * surface centralized so that translation units do not need to duplicate
 * fallback shims.  The implementation preserves overlap semantics when the
 * native memmove() is unavailable.
 */
SIXEL_COMPAT_API void *
sixel_compat_memmove(void *destination,
                     void const *source,
                     size_t byte_count)
{
#if !HAVE_MEMMOVE
    unsigned char *dst_bytes;
    unsigned char const *src_bytes;
    size_t index;
#endif

    if (destination == NULL || source == NULL) {
        return destination;
    }

#if HAVE_MEMMOVE
    return memmove(destination, source, byte_count);
#else
    dst_bytes = (unsigned char *)destination;
    src_bytes = (unsigned char const *)source;
    if (dst_bytes < src_bytes) {
        for (index = 0u; index < byte_count; ++index) {
            dst_bytes[index] = src_bytes[index];
        }
    } else if (dst_bytes > src_bytes) {
        index = byte_count;
        while (index > 0u) {
            index -= 1u;
            dst_bytes[index] = src_bytes[index];
        }
    }

    return destination;
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
# if defined(__GLIBC__) && defined(_GNU_SOURCE)
    /*
     * glibc exposes the GNU strerror_r() that yields a char*. Other libc
     * implementations keep the XSI signature returning int, even when
     * _GNU_SOURCE is defined. Keep the char* path glibc-only to avoid
     * pointer/integer mismatches on macOS and BSD.
     */
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
# if defined(__GLIBC__) && defined(_GNU_SOURCE)
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


/*
 * Normalize incoming paths for the active CRT. This is required for
 * Cygwin/MSYS paths when running MSVC-ABI binaries under Cygwin, because
 * the CRT only understands Windows-style drive paths.
 */
static int
sixel_compat_prepare_path(char const *path,
                          char **buffer_out,
                          char const **libc_path_out)
{
    size_t buffer_size;
    char *buffer;
    char const *libc_path;

    buffer_size = 0u;
    buffer = NULL;
    libc_path = NULL;

    if (path == NULL || buffer_out == NULL || libc_path_out == NULL) {
        errno = EINVAL;
        return (-1);
    }

    buffer_size = sixel_path_to_libc_buffer_size(path);
    if (buffer_size > 0u) {
        buffer = (char *)malloc(buffer_size);
        if (buffer == NULL) {
            errno = ENOMEM;
            return (-1);
        }
        libc_path = sixel_path_to_libc(path, buffer, buffer_size);
        if (libc_path == NULL) {
            free(buffer);
            errno = EINVAL;
            return (-1);
        }
    } else {
        libc_path = path;
    }

    *buffer_out = buffer;
    *libc_path_out = libc_path;
    return 0;
}


SIXEL_COMPAT_API FILE *
sixel_compat_fopen(const char *filename, const char *mode)
{
    FILE *handle;
    char *buffer;
    char const *libc_path;

    handle = NULL;
    buffer = NULL;
    libc_path = NULL;
    if (filename == NULL || mode == NULL) {
        errno = EINVAL;
        return NULL;
    }

#if defined(_MSC_VER)
    if (sixel_compat_prepare_path(filename, &buffer, &libc_path) < 0) {
        return NULL;
    }
#else
    libc_path = filename;
#endif

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
    handle = _fsopen(libc_path, mode, _SH_DENYNO);
#else
    handle = fopen(libc_path, mode);
#endif

    if (buffer != NULL) {
        free(buffer);
    }

    return handle;
}


SIXEL_COMPAT_API const char *
sixel_compat_getenv(const char *name)
{
#if HAVE__DUPENV_S || defined(_MSC_VER)
    struct sixel_env_cache {
        char *name;
        char *value;
        struct sixel_env_cache *next;
    };
    static struct sixel_env_cache *cache_head = NULL;
    struct sixel_env_cache *entry;
    struct sixel_env_cache *new_entry;
    size_t name_length;
    char *value;
    char *name_copy;
    char *value_copy;
    size_t length;
    int copy_result;
    errno_t status;

    if (name == NULL) {
        return NULL;
    }

    entry = cache_head;
    while (entry != NULL) {
        if (strcmp(entry->name, name) == 0) {
            break;
        }
        entry = entry->next;
    }

    value = NULL;
    length = 0u;
    status = 0;

    /*
     * `_dupenv_s()` allocates the buffer for us and reports the byte count
     * in `length`.  Cache a dedicated copy per variable name so multiple
     * lookups can coexist without sharing a single static buffer.
     */
    status = _dupenv_s(&value, &length, name);
    if (status != 0) {
        if (value != NULL) {
            free(value);
        }
        return NULL;
    }
    if (value == NULL) {
        return NULL;
    }
    value_copy = (char *)malloc(length + 1);
    if (value_copy == NULL) {
        free(value);
        return NULL;
    }
    memcpy(value_copy, value, length);
    value_copy[length] = '\0';
    free(value);

    if (entry != NULL) {
        free(entry->value);
        entry->value = value_copy;
        return entry->value;
    }

    new_entry = (struct sixel_env_cache *)malloc(sizeof(*new_entry));
    if (new_entry == NULL) {
        free(value_copy);
        return NULL;
    }

    name_length = strlen(name);
    name_copy = (char *)malloc(name_length + 1);
    if (name_copy == NULL) {
        free(value_copy);
        free(new_entry);
        return NULL;
    }
    /*
     * Copy the variable name via the compat helper so MSVC uses
     * strcpy_s() without emitting deprecation warnings.
     */
    copy_result = sixel_compat_strcpy(name_copy,
                                      name_length + 1,
                                      name);
    if (copy_result < 0) {
        free(name_copy);
        free(value_copy);
        free(new_entry);
        return NULL;
    }

    new_entry->name = name_copy;
    new_entry->value = value_copy;
    new_entry->next = cache_head;
    cache_head = new_entry;

    return new_entry->value;
#else
    return getenv(name);
#endif
}


SIXEL_COMPAT_API int
sixel_compat_isatty(int fd)
{
#if defined(_WIN32)
    return _isatty(fd);
#else
    return isatty(fd);
#endif
}


/*
 * Console detection helper
 *
 * Windows reports the NUL device as a TTY, so callers that need an
 * interactive console must verify the handle against GetConsoleMode().
 * Centralizing the probe keeps feature-macro gated CRT calls in one
 * translation unit for the unity build.
 */
SIXEL_COMPAT_API int
sixel_compat_is_console(int fd)
{
#if defined(_WIN32)
    intptr_t handle;
    DWORD mode;

    if (fd < 0) {
        return 0;
    }

    if (!sixel_compat_isatty(fd)) {
        return 0;
    }

    handle = _get_osfhandle(fd);
    if (handle == (intptr_t)(-1)) {
        return 0;
    }

    if (GetConsoleMode((HANDLE)handle, &mode) != 0) {
        return 1;
    }

    return 0;
#else
# if HAVE_ISATTY
    if (fd < 0) {
        return 0;
    }

    return sixel_compat_isatty(fd);
# else
    (void)fd;

    return 0;
# endif
#endif
}


/*
 * Portable gettimeofday() wrapper used by timer helpers.
 * +----------------------+------------------------------------------+
 * | Platform             | Strategy                                 |
 * +----------------------+------------------------------------------+
 * | Windows              | FILETIME converted to Unix epoch         |
 * | POSIX gettimeofday() | Delegate to libc                         |
 * | clock_gettime() only | CLOCK_REALTIME mapped into struct timeval|
 * | time() fallback      | Second resolution when nothing else      |
 * +----------------------+------------------------------------------+
 */
SIXEL_COMPAT_API int
sixel_compat_gettimeofday(struct timeval *tv)
{
    int status;
#if defined(_WIN32)
    FILETIME file_time;
    ULARGE_INTEGER ticks;
    const ULONGLONG epoch_offset = 116444736000000000ULL;
#elif defined(HAVE_GETTIMEOFDAY)
    /* storage not needed for this branch */
#elif defined(HAVE_CLOCK_GETTIME)
    struct timespec ts;
#else
    time_t seconds;
#endif

    status = (-1);
    if (tv == NULL) {
        errno = EINVAL;
        return status;
    }

#if defined(_WIN32)
    GetSystemTimeAsFileTime(&file_time);
    ticks.LowPart = file_time.dwLowDateTime;
    ticks.HighPart = file_time.dwHighDateTime;
    if (ticks.QuadPart < epoch_offset) {
        errno = EINVAL;
        return status;
    }
    ticks.QuadPart -= epoch_offset;
    tv->tv_sec = (long)(ticks.QuadPart / 10000000ULL);
    tv->tv_usec = (long)((ticks.QuadPart / 10ULL) % 1000000ULL);
    return 0;
#elif defined(HAVE_GETTIMEOFDAY)
    status = gettimeofday(tv, NULL);
    return status;
#elif defined(HAVE_CLOCK_GETTIME)
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
        return status;
    }
    tv->tv_sec = (long)ts.tv_sec;
    tv->tv_usec = (long)(ts.tv_nsec / 1000L);
    return 0;
#else
    seconds = time(NULL);
    if (seconds == (time_t)(-1)) {
        return status;
    }
    tv->tv_sec = (long)seconds;
    tv->tv_usec = 0L;
    return 0;
#endif
}


SIXEL_COMPAT_API int
sixel_compat_setenv(const char *name, const char *value)
{
#if defined(_WIN32)
    errno_t status;

    status = 0;
    if (name == NULL || value == NULL) {
        errno = EINVAL;
        return (-1);
    }

    status = _putenv_s(name, value);
    if (status != 0) {
        errno = status;
        return (-1);
    }

    return 0;
#else
    if (name == NULL || value == NULL) {
        errno = EINVAL;
        return (-1);
    }

    if (setenv(name, value, 1) != 0) {
        return (-1);
    }

    return 0;
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


SIXEL_COMPAT_API char *
sixel_compat_tmpnam(char *buffer, size_t buffer_size)
{
#if defined(_MSC_VER)
    int status;

    status = 0;
#elif HAVE_MKSTEMP
    int descriptor;

    descriptor = (-1);
#endif
    if (buffer == NULL || buffer_size == 0u) {
        errno = EINVAL;
        return NULL;
    }

#if defined(_MSC_VER)
    status = tmpnam_s(buffer, buffer_size);
    if (status != 0) {
        errno = status;
        return NULL;
    }
    return buffer;
#else
    (void)buffer_size;
# if HAVE_MKSTEMP
    descriptor = mkstemp(buffer);
    if (descriptor < 0) {
        return NULL;
    }
    (void)sixel_compat_close(descriptor);
    descriptor = (-1);
    if (sixel_compat_unlink(buffer) != 0) {
        return NULL;
    }
    return buffer;
# else
    return tmpnam(buffer);
# endif
#endif
}


SIXEL_COMPAT_API struct tm *
sixel_compat_localtime(const time_t *timer, struct tm *result)
{
    struct tm *converted;

    converted = NULL;
    if (timer == NULL) {
        errno = EINVAL;
        return NULL;
    }

#if defined(_MSC_VER)
    if (result == NULL) {
        errno = EINVAL;
        return NULL;
    }
    if (localtime_s(result, timer) != 0) {
        return NULL;
    }
    converted = result;
#elif defined(HAVE_LOCALTIME_R)
    if (result == NULL) {
        errno = EINVAL;
        return NULL;
    }
    converted = localtime_r(timer, result);
#else
    (void)result;
    converted = localtime(timer);
#endif

    return converted;
}


SIXEL_COMPAT_API int
sixel_compat_vfprintf(FILE *stream, const char *format, va_list args)
{
    int written;

    written = (-1);
    if (stream == NULL || format == NULL) {
        return written;
    }

#if defined(_MSC_VER) || defined(__STDC_LIB_EXT1__)
    /*
     * vfprintf_s enforces runtime format validation and rejects mismatches
     * that classic vfprintf would allow. The caller validates the format
     * string, so temporarily silence the -Wformat-nonliteral warning
     * triggered by storing the format in a variable. MSVC links this entry
     * point as vfprintf_s (no leading underscore), while Annex K platforms
     * also expose the same name.
     */
# if HAVE_DIAGNOSTIC_FORMAT_NONLITERAL
#  if defined(__GNUC__) && !defined(__PCC__)
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wformat-nonliteral"
#  endif
# endif
    written = vfprintf_s(stream, format, args);
# if HAVE_DIAGNOSTIC_FORMAT_NONLITERAL
#  if defined(__GNUC__) && !defined(__PCC__)
#   pragma GCC diagnostic pop
#  endif
# endif
#else
    /*
     * POSIX vfprintf emits the same warning when the format string is
     * stored outside a literal. Limit the suppression to this narrow call
     * site to keep diagnostics useful elsewhere.
     */
# if HAVE_DIAGNOSTIC_FORMAT_NONLITERAL
#  if defined(__GNUC__) && !defined(__PCC__)
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wformat-nonliteral"
#  endif
# endif
    written = vfprintf(stream, format, args);
# if HAVE_DIAGNOSTIC_FORMAT_NONLITERAL
#  if defined(__GNUC__) && !defined(__PCC__)
#   pragma GCC diagnostic pop
#  endif
# endif
#endif

    return written;
}


SIXEL_COMPAT_API int
sixel_compat_vsscanf(const char *buffer, const char *format, va_list args)
{
    if (buffer == NULL || format == NULL) {
        return (-1);
    }

#if defined(_MSC_VER) || defined(__STDC_LIB_EXT1__)
    /*
     * vsscanf_s requires a literal format string, but our callers pass
     * validated format strings held in variables. Suppress the
     * -Wformat-nonliteral warning in this narrow scope while keeping the
     * safer CRT entry point.
     */
# if HAVE_DIAGNOSTIC_FORMAT_NONLITERAL
#  if defined(__GNUC__) && !defined(__PCC__)
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wformat-nonliteral"
#  endif
# endif
    return vsscanf_s(buffer, format, args);
# if HAVE_DIAGNOSTIC_FORMAT_NONLITERAL
#  if defined(__GNUC__) && !defined(__PCC__)
#   pragma GCC diagnostic pop
#  endif
# endif
#else
    /*
     * POSIX vsscanf also triggers -Wformat-nonliteral when the format is
     * kept in a variable. The format string is verified by callers, so
     * temporarily silence the warning only around this call.
     */
# if HAVE_DIAGNOSTIC_FORMAT_NONLITERAL
#  if defined(__GNUC__) && !defined(__PCC__)
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wformat-nonliteral"
#  endif
# endif
    return vsscanf(buffer, format, args);
# if HAVE_DIAGNOSTIC_FORMAT_NONLITERAL
#  if defined(__GNUC__) && !defined(__PCC__)
#   pragma GCC diagnostic pop
#  endif
# endif
#endif
}


SIXEL_COMPAT_API int
sixel_compat_sscanf(const char *buffer, const char *format, ...)
{
    va_list args;
    int parsed;

    parsed = (-1);
    if (buffer == NULL || format == NULL) {
        return parsed;
    }

    va_start(args, format);
    parsed = sixel_compat_vsscanf(buffer, format, args);
    va_end(args);

    return parsed;
}


SIXEL_COMPAT_API int
sixel_compat_mktemp(char *templ, size_t buffer_size)
{
#if defined(_MSC_VER)
    errno_t error;

    if (templ == NULL || buffer_size == 0) {
        errno = EINVAL;
        return (-1);
    }
    error = _mktemp_s(templ, buffer_size);
    if (error != 0) {
        errno = error;
        return (-1);
    }
    return 0;
#elif HAVE_MKSTEMP
    int fd;

    (void)buffer_size;

    if (templ == NULL) {
        errno = EINVAL;
        return (-1);
    }
    fd = mkstemp(templ);
    if (fd >= 0) {
        (void)sixel_compat_close(fd);
        (void)sixel_compat_unlink(templ);
        return 0;
    }
    return (-1);
#elif HAVE_MKTEMP
    (void)buffer_size;

    if (templ == NULL) {
        errno = EINVAL;
        return (-1);
    }
    if (mktemp(templ) == NULL) {
        return (-1);
    }
    return 0;
#else
    (void)templ;
    (void)buffer_size;
    errno = ENOSYS;
    return (-1);
#endif
}


SIXEL_COMPAT_API int
sixel_compat_open(const char *path, int flags, ...)
{
    int fd;
    va_list args;
    int mode;
    char *buffer;
    char const *libc_path;
#if defined(_MSC_VER) && defined(HAVE__SOPEN_S) && HAVE__SOPEN_S
    errno_t err;
    int share_flags;
#endif

    fd = (-1);
    mode = 0;
    buffer = NULL;
    libc_path = NULL;

    if (path == NULL) {
        errno = EINVAL;
        return (-1);
    }

    va_start(args, flags);
    if (flags & O_CREAT) {
        mode = va_arg(args, int);
    }
    va_end(args);

#if defined(_MSC_VER)
    if (sixel_compat_prepare_path(path, &buffer, &libc_path) < 0) {
        return (-1);
    }
#else
    libc_path = path;
#endif

#if defined(_MSC_VER) && defined(HAVE__SOPEN_S) && HAVE__SOPEN_S
    /*
     * Prefer the secure CRT entry point when available.  _sopen_s reports
     * errors through errno_t. Translate failures back into errno values for
     * the public API.
     */
    share_flags = _SH_DENYNO;
    err = _sopen_s(&fd, libc_path, flags, share_flags, mode);
    if (err != 0) {
        errno = (int)err;
        fd = (-1);
    }
#elif defined(_MSC_VER) && defined(HAVE__OPEN) && HAVE__OPEN
# pragma warning(push)
# pragma warning(disable : 4996)
    fd = _open(libc_path, flags, mode);
# pragma warning(pop)
#elif defined(HAVE_OPEN) && HAVE_OPEN
    if (flags & O_CREAT) {
        fd = open(libc_path, flags, (mode_t)mode);
    } else {
        fd = open(libc_path, flags);
    }
#else
    errno = ENOSYS;
    fd = (-1);
#endif

    if (buffer != NULL) {
        free(buffer);
    }

    return fd;
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
    char *buffer;
    char const *libc_path;
    int result;

    buffer = NULL;
    libc_path = NULL;
    result = (-1);

    if (path == NULL) {
        errno = EINVAL;
        return (-1);
    }

#if defined(_MSC_VER)
    if (sixel_compat_prepare_path(path, &buffer, &libc_path) < 0) {
        return (-1);
    }
#else
    libc_path = path;
#endif

#if defined(_MSC_VER)
    result = _unlink(libc_path);
#else
    result = unlink(libc_path);
#endif

    if (buffer != NULL) {
        free(buffer);
    }

    return result;
}


/*
 * Force file descriptors into binary mode when the platform exposes the
 * corresponding flag.  This keeps Windows stdio from translating newlines
 * while leaving POSIX callers untouched.
 */
SIXEL_COMPAT_API int
sixel_compat_set_binary(int fd)
{
    int result;

    result = 0;
#if defined(O_BINARY)
# if defined(SIXEL_HAVE_WIN_SETMODE)
    result = _setmode(fd, O_BINARY);
# elif HAVE_SETMODE
    result = setmode(fd, O_BINARY);
# else
    (void)fd;
# endif
#else
    (void)fd;
#endif

    return result;
}


SIXEL_COMPAT_API int
sixel_compat_access(const char *path, int mode)
{
    size_t buffer_size;
    char *buffer;
    char const *libc_path;
    int result;

    buffer_size = 0u;
    buffer = NULL;
    libc_path = NULL;
    result = -1;
    if (path == NULL) {
        errno = EINVAL;
        return -1;
    }

    buffer_size = sixel_path_to_libc_buffer_size(path);
    if (buffer_size > 0u) {
        buffer = (char *)malloc(buffer_size);
        if (buffer == NULL) {
            errno = ENOMEM;
            return -1;
        }
        libc_path = sixel_path_to_libc(path, buffer, buffer_size);
        if (libc_path == NULL) {
            free(buffer);
            errno = EINVAL;
            return -1;
        }
    } else {
        libc_path = path;
    }

#if defined(_MSC_VER)
    result = _access(libc_path, mode);
#else
    result = access(libc_path, mode);
#endif

    if (buffer != NULL) {
        free(buffer);
    }

    return result;
}


SIXEL_COMPAT_API int
sixel_compat_stat(const char *path, struct stat *stat_buffer)
{
    size_t buffer_size;
    char *buffer;
    char const *libc_path;
    int result;

    buffer_size = 0u;
    buffer = NULL;
    libc_path = NULL;
    result = -1;
    if (path == NULL || stat_buffer == NULL) {
        errno = EINVAL;
        return -1;
    }

    buffer_size = sixel_path_to_libc_buffer_size(path);
    if (buffer_size > 0u) {
        buffer = (char *)malloc(buffer_size);
        if (buffer == NULL) {
            errno = ENOMEM;
            return -1;
        }
        libc_path = sixel_path_to_libc(path, buffer, buffer_size);
        if (libc_path == NULL) {
            free(buffer);
            errno = EINVAL;
            return -1;
        }
    } else {
        libc_path = path;
    }

#if defined(_MSC_VER)
    /*
     * Use the MSVC-specific stat variant that matches the active time_t
     * size to avoid warning-as-error mismatches under /WX.
     */
# if defined(_USE_32BIT_TIME_T)
    result = _stat64i32(libc_path, (struct _stat64i32 *)stat_buffer);
# else
    result = _stat64(libc_path, (struct _stat64 *)stat_buffer);
# endif
#else
    result = stat(libc_path, stat_buffer);
#endif

    if (buffer != NULL) {
        free(buffer);
    }

    return result;
}


SIXEL_COMPAT_API ssize_t
sixel_compat_write(int fd, const void *buffer, size_t count)
{
#if defined(_MSC_VER)
    return (ssize_t)_write(fd, buffer, (unsigned int)count);
#else
    return write(fd, buffer, count);
#endif
}


static char *
sixel_compat_resolve_without_realpath(const char *path)
{
    char *cwd;
    char *resolved;
    size_t cwd_length;
    size_t path_length;
    int need_separator;

    cwd = NULL;
    resolved = NULL;
    cwd_length = 0;
    path_length = 0;
    need_separator = 0;

    if (path == NULL) {
        return NULL;
    }

    if (path[0] == '/') {
        path_length = strlen(path);
        resolved = malloc(path_length + 1);
        if (resolved == NULL) {
            return NULL;
        }
        memcpy(resolved, path, path_length + 1);

        return resolved;
    }

#if defined(PATH_MAX)
    cwd = malloc(PATH_MAX);
    if (cwd != NULL) {
        if (sixel_compat_getcwd(cwd, PATH_MAX) != NULL) {
            cwd_length = strlen(cwd);
            path_length = strlen(path);
            if (cwd_length > 0 && cwd[cwd_length - 1] != '/') {
                need_separator = 1;
            }
            resolved = malloc(cwd_length + need_separator + path_length + 1);
            if (resolved != NULL) {
                memcpy(resolved, cwd, cwd_length);
                if (need_separator != 0) {
                    resolved[cwd_length] = '/';
                }
                memcpy(resolved + cwd_length + need_separator,
                       path,
                       path_length + 1);
            }
        }
        free(cwd);
        if (resolved != NULL) {
            return resolved;
        }
    }
#else
    (void)cwd;
    (void)cwd_length;
    (void)need_separator;
#endif  /* PATH_MAX */

    path_length = strlen(path);
    resolved = malloc(path_length + 1);
    if (resolved == NULL) {
        return NULL;
    }
    memcpy(resolved, path, path_length + 1);

    return resolved;
}


/*
 * Map platform-specific realpath implementations into a single helper that
 * can be reused across modules.  The fallback path joins CWD and the input
 * when neither POSIX nor CRT helpers are available.
 */
SIXEL_COMPAT_API char *
sixel_compat_realpath(const char *path)
{
    char *resolved;

    resolved = NULL;

    if (path == NULL) {
        return NULL;
    }

#if defined(HAVE_REALPATH)
    /* Prefer the POSIX implementation when available. */
    resolved = realpath(path, NULL);
#elif defined(_WIN32) && defined(HAVE__FULLPATH)
    resolved = _fullpath(NULL, path, 0);
#elif defined(_WIN32) && defined(HAVE__REALPATH)
    /*
     * Some Windows SDKs offer `_realpath()` as a twin to the POSIX
     * function.  Limit the call to Windows builds to avoid mis-detected
     * probes on other platforms.
     */
    resolved = _realpath(path, NULL);
#else
    resolved = sixel_compat_resolve_without_realpath(path);
#endif

    if (resolved == NULL) {
        resolved = sixel_compat_resolve_without_realpath(path);
    }

    return resolved;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
