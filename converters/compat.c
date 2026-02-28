/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#if !defined(_POSIX_C_SOURCE)
# define _POSIX_C_SOURCE 200809L
#endif

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include "compat.h"
#include "path.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if HAVE_ERRNO_H
# include <errno.h>
#endif  /* HAVE_ERRNO_H */
#if HAVE_STDARG_H
# include <stdarg.h>
#endif  /* HAVE_STDARG_H */
#if HAVE_FCNTL_H
# include <fcntl.h>
#endif  /* HAVE_FCNTL_H */
#if HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif  /* HAVE_SYS_STAT_H */
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif  /* HAVE_SYS_TYPES_H */
#if HAVE_UNISTD_H
# include <unistd.h>
#endif  /* HAVE_UNISTD_H */

/* _WIN32 */
#if HAVE_DIRECT_H
# include <direct.h>
#endif  /* HAVE_DIRECT_H */
#if HAVE_IO_H
# include <io.h>
#endif  /* HAVE_IO_H */
#if defined(_MSC_VER)
# if !defined(WIN32_LEAN_AND_MEAN)
#  define WIN32_LEAN_AND_MEAN
# endif
# include <windows.h>
# include <share.h>
#endif  /* _MSC_VER */

/* for msvc */
#ifndef STDIN_FILENO
# define STDIN_FILENO 0
#endif
#ifndef STDOUT_FILENO
# define STDOUT_FILENO 1
#endif
#ifndef STDERR_FILENO
# define STDERR_FILENO 2
#endif

/* Provide ssize_t so MSVC matches POSIX I/O signatures. */
#if defined(_MSC_VER) && !defined(_SSIZE_T_DEFINED)
# include <BaseTsd.h>
typedef SSIZE_T ssize_t;
# define _SSIZE_T_DEFINED
#endif

/* ------------------------------------------------------------------------ */
/* MSVC exposes the classic POSIX permission bits with _S_* names.  To      */
/* document the mapping we draw the alias ladder so anyone porting new      */
/* callers understands that the left side feeds the right side:             */
/*                                                                          */
/*   S_IRUSR  --->  _S_IREAD                                                */
/*   S_IWUSR  --->  _S_IWRITE                                               */
/*                                                                          */
/* The quick diagram keeps the intent readable while making the macros      */
/* available to code that expects the POSIX spellings.                      */
/* ------------------------------------------------------------------------ */
#if defined(_MSC_VER)
# if !defined(S_IRUSR)
#  define S_IRUSR _S_IREAD
# endif
# if !defined(S_IWUSR)
#  define S_IWUSR _S_IWRITE
# endif
#endif

/* Replicate POSIX access() flag for readability. */
#if !defined(R_OK)
# define R_OK 4
#endif
#if !defined(F_OK)
# define F_OK 0
#endif

/* ------------------------------------------------------------------------ */
/* the hybrid lookup strategy:                                              */
/*                                                                          */
/*   +--------------------------+ yes  +------------------------------+     */
/*   | env IMG2SIXEL_*=paths?   |----->| use pointed file             |     */
/*   +--------------------------+      +------------------------------+     */
/*                    | no                                                  */
/*                    v                                                     */
/*   +------------------------------+ yes +------------------------------+  */
/*   | pkgdatadir candidates exist? |---->| use first readable candidate |  */
/*   +------------------------------+     +------------------------------+  */
/*                    | no                                                  */
/*                    v                                                     */
/*         +-------------------+                                            */
/*         | embedded fallback |                                            */
/*         +-------------------+                                            */
/*                                                                          */
/* Each block prefers the earliest successful source.                       */
/* ------------------------------------------------------------------------ */

#if defined(HAVE_MKSTEMP)
int mkstemp(char *);
#endif

#if !defined(_WIN32)
int fchmod(int, mode_t);
#endif

/* ------------------------------------------------------------------------ */
/* private copies of the compat helpers we consume                          */
/*                                                                          */
/* The completion binary must not reach into src/compat_stub.c because      */
/* amalgamation mode concatenates all sources into a single file.  To avoid */
/* clashing with the library's shim symbols we keep a tool-specific         */
/* prefix.                                                                  */
/* ------------------------------------------------------------------------ */

#if defined(_MSC_VER)
static int img2sixel_compat_strcpy(char *destination,
                                   size_t destination_size,
                                   const char *source);
#endif
static void img2sixel_compat_log_errno(const char *fmt, ...);
char *
img2sixel_compat_strerror(int error_number,
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
# if defined(__GLIBC__) && defined(_GNU_SOURCE) && !defined(__APPLE__)
    char *message;
    size_t copy_length;
# else
    int status;
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
    copy_length = strlen(message);
    if (copy_length >= buffer_size) {
        copy_length = buffer_size - 1;
    }
    memcpy(buffer, message, copy_length);
    buffer[copy_length] = '\0';
    return buffer;
# endif
#else
# if defined(__GLIBC__) && defined(_GNU_SOURCE) && !defined(__APPLE__)
    /* GNU strerror_r returns the error message pointer. */
    message = strerror_r(error_number, buffer, buffer_size);
    if (message == NULL) {
        buffer[0] = '\0';
        return NULL;
    }
    copy_length = strlen(message);
    if (copy_length >= buffer_size) {
        copy_length = buffer_size - 1;
    }
    memcpy(buffer, message, copy_length);
    buffer[copy_length] = '\0';
    return buffer;
# else
    /* POSIX strerror_r returns 0 on success and non-zero on failure. */
    status = strerror_r(error_number, buffer, buffer_size);
    if (status != 0) {
        buffer[0] = '\0';
        return NULL;
    }
    return buffer;
# endif
#endif
}

/*
 * Normalize incoming paths for the current platform. The converter builds
 * cannot rely on src/path.c, so we keep a local copy of the logic and only
 * use it when conversion is necessary.
 */
int
img2sixel_compat_prepare_path(char const *path,
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

    /*
     * Keep behavior equivalent to the pre-split helper in completion_utils.c
     * so path conversion remains strict on Windows-hosted environments.
     */
/*
 * Amalgamation rewrites core path helpers with the sixel_ prefix in src/
 * sources, while split converter builds keep the img2sixel_ prefix.
 */
#if defined(SIXEL_AMALGAMATION)
    buffer_size = sixel_path_to_libc_buffer_size(path);
#else
    buffer_size = img2sixel_path_to_libc_buffer_size(path);
#endif
    if (buffer_size > 0u) {
        buffer = (char *)malloc(buffer_size);
        if (buffer == NULL) {
            errno = ENOMEM;
            return (-1);
        }
#if defined(SIXEL_AMALGAMATION)
        libc_path = sixel_path_to_libc(path, buffer, buffer_size);
#else
        libc_path = img2sixel_path_to_libc(path, buffer, buffer_size);
#endif
        if (libc_path == NULL) {
            free(buffer);
            buffer = NULL;
            libc_path = path;
        }
    } else {
        libc_path = path;
    }

    *buffer_out = buffer;
    *libc_path_out = libc_path;
    return 0;
}

FILE *
img2sixel_compat_fopen(const char *filename, const char *mode)
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

    if (img2sixel_compat_prepare_path(filename, &buffer, &libc_path) < 0) {
        return NULL;
    }

#if defined(_MSC_VER)
    handle = _fsopen(libc_path, mode, _SH_DENYNO);
#else
    handle = fopen(libc_path, mode);
#endif

    if (buffer != NULL) {
        free(buffer);
    }

    return handle;
}

const char *
img2sixel_compat_getenv(const char *name)
{
#if defined(_MSC_VER)
    struct img2sixel_env_cache {
        char *name;
        char *value;
        struct img2sixel_env_cache *next;
    };
    static struct img2sixel_env_cache *cache_head = NULL;
    struct img2sixel_env_cache *entry;
    struct img2sixel_env_cache *new_entry;
    char *value;
    char *name_copy;
    char *value_copy;
    int copy_result;
    size_t length;
    size_t name_length;

    if (name == NULL || name[0] == '\0') {
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

    new_entry = (struct img2sixel_env_cache *)malloc(sizeof(*new_entry));
    if (new_entry == NULL) {
        free(value_copy);
        return NULL;
    }

    name_length = strlen(name) + 1;
    if (name_length <= 1) {
        free(value_copy);
        free(new_entry);
        return NULL;
    }

    name_copy = (char *)malloc(name_length);
    if (name_copy == NULL) {
        free(value_copy);
        free(new_entry);
        return NULL;
    }
    copy_result = img2sixel_compat_strcpy(name_copy,
                                          name_length,
                                          name);
    if (copy_result < 0) {
        free(value_copy);
        free(new_entry);
        free(name_copy);
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

int
img2sixel_compat_setenv(const char *name, const char *value)
{
#if defined(_MSC_VER)
    errno_t status;

    if (name == NULL || value == NULL) {
        errno = EINVAL;
        return -1;
    }

    if (SetEnvironmentVariableA(name, value) == 0) {
        errno = EINVAL;
        return -1;
    }

    status = _putenv_s(name, value);
    if (status != 0) {
        errno = status;
        return -1;
    }

    return 0;
#else
    if (name == NULL || value == NULL) {
        errno = EINVAL;
        return -1;
    }

    return setenv(name, value, 1);
#endif
}

#if defined(_MSC_VER)
/*
 * Provide a local copy helper so MSVC builds can use strcpy_s()
 * without sprinkling deprecation warnings through the code.
 */
static int
img2sixel_compat_strcpy(char *destination,
                        size_t destination_size,
                        const char *source)
{
    size_t length;
    size_t required_size;

    if (destination == NULL || source == NULL || destination_size == 0) {
        errno = EINVAL;
        return (-1);
    }

#if defined(HAVE_STRCPY_S)
    (void) required_size;
    if (strcpy_s(destination, destination_size, source) != 0) {
        errno = EINVAL;
        return (-1);
    }
    length = strlen(destination);
    return (int)length;
#else
    length = strlen(source);
    required_size = length + 1;
    /*
     * Mirror strcpy_s() semantics and fail on truncation so callers
     * can distinguish a real copy from a clipped buffer.
     */
    if (required_size > destination_size) {
        errno = ERANGE;
        return (-1);
    }
    memcpy(destination, source, required_size);
    return (int)length;
#endif
}
#endif

static void
img2sixel_compat_log_errno(const char *fmt, ...)
{
    va_list ap;
    int written;
    char errbuf[128];
    char message[512];

    written = 0;
    /*
     * Format into a fixed buffer so that compilers never treat the variadic
     * format as unknown and warn about non-literal usage.
     */
    memset(message, 0, sizeof(message));

    va_start(ap, fmt);
    /*
     * Fortified stdio wrappers warn about non-literal formats; the
     * completion strings are fixed at generation time, so silence the
     * diagnostic while still bounding the output.
     */
#if HAVE_DIAGNOSTIC_FORMAT_NONLITERAL
# if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wformat-nonliteral"
# elif defined(__GNUC__) && !defined(__PCC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wformat-nonliteral"
# endif
#endif
    written = vsnprintf(message, sizeof(message), fmt, ap);
#if HAVE_DIAGNOSTIC_FORMAT_NONLITERAL
# if defined(__clang__)
#  pragma clang diagnostic pop
# elif defined(__GNUC__) && !defined(__PCC__)
#  pragma GCC diagnostic pop
# endif
#endif
    va_end(ap);

    if (written < 0) {
        fputs("log formatting failed", stderr);
    } else if ((size_t)written >= sizeof(message)) {
        fputs(message, stderr);
        fputs("...", stderr);
    } else {
        fputs(message, stderr);
    }
    if (errno != 0) {
        if (img2sixel_compat_strerror(errno, errbuf, sizeof(errbuf)) != NULL) {
            fprintf(stderr, ": %s", errbuf);
        } else {
            fprintf(stderr, ": errno=%d", errno);
        }
    }
    fprintf(stderr, "\n");
}

/*
 * Use the platform-specific chmod entry point so MSVC does not warn
 * about the POSIX spelling.
 */
int
img2sixel_compat_chmod(const char *path, mode_t mode)
{
    char *buffer;
    char const *libc_path;

    buffer = NULL;
    libc_path = NULL;

    if (img2sixel_compat_prepare_path(path, &buffer, &libc_path) < 0) {
        return (-1);
    }

#if defined(_MSC_VER) && defined(HAVE__CHMOD)
    if (_chmod(libc_path, (int)mode) != 0) {
        img2sixel_compat_log_errno("chmod failed path=%s libc_path=%s",
                                   path, libc_path);
        if (buffer != NULL) {
            free(buffer);
        }
        return (-1);
    }
#elif defined(HAVE_CHMOD)
    if (chmod(libc_path, mode) != 0) {
        img2sixel_compat_log_errno("chmod failed path=%s libc_path=%s",
                                   path, libc_path);
        if (buffer != NULL) {
            free(buffer);
        }
        return (-1);
    }
#else
    (void)path;
    (void)mode;
    errno = ENOSYS;
    if (buffer != NULL) {
        free(buffer);
    }
    return (-1);
#endif

    if (buffer != NULL) {
        free(buffer);
    }
    return 0;
}

#if !defined(HAVE_MKSTEMP)
int
img2sixel_compat_mktemp(char *templ, size_t buffer_size)
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

int
img2sixel_compat_open(const char *path, int flags, ...)
{
    int fd;
    va_list args;
    int mode;
    char *buffer;
    char const *libc_path;
#if defined(_MSC_VER)
    errno_t error;
#endif

    fd = (-1);
    mode = 0;
    buffer = NULL;
    libc_path = NULL;

    if (path == NULL) {
        errno = EINVAL;
        return (-1);
    }

    if (img2sixel_compat_prepare_path(path, &buffer, &libc_path) < 0) {
        return (-1);
    }

    va_start(args, flags);
    if (flags & O_CREAT) {
        mode = va_arg(args, int);
    }
    va_end(args);

#if defined(_MSC_VER)
    /*
     * _sopen_s requires an explicit sharing mode.  _SH_DENYNO mirrors the
     * default POSIX behaviour by allowing other processes to access the file
     * while keeping the rest of the flag set intact.
     */
    error = _sopen_s(&fd, libc_path, flags, _SH_DENYNO, mode);
    if (error != 0) {
        fd = (-1);
    }
#else
    if (flags & O_CREAT) {
        fd = open(libc_path, flags, (mode_t)mode);
    } else {
        fd = open(libc_path, flags);
    }
#endif

    if (buffer != NULL) {
        free(buffer);
    }

    return fd;
}
#endif  /* !HAVE_MKSTEMP */

int
img2sixel_compat_close(int fd)
{
#if defined(_MSC_VER)
    return _close(fd);
#else
    return close(fd);
#endif
}

int
img2sixel_compat_unlink(const char *path)
{
    char *buffer;
    char const *libc_path;

    buffer = NULL;
    libc_path = NULL;

    if (img2sixel_compat_prepare_path(path, &buffer, &libc_path) < 0) {
        return (-1);
    }

#if defined(_MSC_VER)
    if (_unlink(libc_path) != 0) {
        if (buffer != NULL) {
            free(buffer);
        }
        return (-1);
    }
#else
    if (unlink(libc_path) != 0) {
        if (buffer != NULL) {
            free(buffer);
        }
        return (-1);
    }
#endif
    if (buffer != NULL) {
        free(buffer);
    }
    return 0;
}

int
img2sixel_compat_access(const char *path, int mode)
{
    char *buffer;
    char const *libc_path;

    buffer = NULL;
    libc_path = NULL;

    if (img2sixel_compat_prepare_path(path, &buffer, &libc_path) < 0) {
        return (-1);
    }

#if defined(_MSC_VER)
    if (_access(libc_path, mode) != 0) {
        if (buffer != NULL) {
            free(buffer);
        }
        return (-1);
    }
#else
    if (access(libc_path, mode) != 0) {
        if (buffer != NULL) {
            free(buffer);
        }
        return (-1);
    }
#endif
    if (buffer != NULL) {
        free(buffer);
    }
    return 0;
}

#if defined(_MSC_VER)

static int
safe__win32_error_to_errno(DWORD e)
{
    switch (e) {
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
    case ERROR_INVALID_DRIVE:
    case ERROR_INVALID_NAME:
    case ERROR_BAD_PATHNAME:
        return ENOENT;
    case ERROR_DIRECTORY:
        return ENOTDIR;
    case ERROR_ACCESS_DENIED:
    case ERROR_SHARING_VIOLATION:
    case ERROR_LOCK_VIOLATION:
        return EACCES;
    case ERROR_FILENAME_EXCED_RANGE:
        return ENAMETOOLONG;
    case ERROR_NOT_ENOUGH_MEMORY:
    case ERROR_OUTOFMEMORY:
        return ENOMEM;
    case ERROR_INVALID_PARAMETER:
        return EINVAL;
    default:
        break;
    }
    return EIO;
}

# ifndef SAFE_STAT64_CODEPAGE
#  define SAFE_STAT64_CODEPAGE CP_UTF8
# endif

static int
safe_stat64W(const wchar_t *path, struct stat *st)
{
    DWORD attr, ge;
    int result;

    if (!path || !st || !*path) {
       errno = EINVAL;
       return (-1);
    }

    attr = GetFileAttributesW(path);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        errno = safe__win32_error_to_errno(GetLastError());
        return (-1);
    }

    /*
     * Mirror the library-side stat wrapper so /WX builds avoid
     * mismatched time_t warnings.
     */
# if defined(_USE_32BIT_TIME_T)
    result = _wstat64i32(path, (struct _stat64i32 *)st);
# else
    result = _wstat64(path, (struct _stat64 *)st);
# endif
    if (result == 0) {
        return 0;
    }

    if (errno == 0) {
        ge = GetLastError();
        errno = ge ? safe__win32_error_to_errno(ge) : EIO;
    }

    return (-1);
}

static int
safe_stat64A(const char *path, struct stat *st)
{
    int wlen;
    int rc;
    wchar_t *w;

    if (! path) {
        errno = EINVAL;
        return (-1);
    }

    wlen = MultiByteToWideChar(SAFE_STAT64_CODEPAGE,
                               MB_ERR_INVALID_CHARS,
                               path, -1, NULL, 0);

    if (wlen <= 0) {
       errno = EINVAL;
       return (-1);
    }

    w = (wchar_t*)malloc((size_t)wlen * sizeof(wchar_t));
    if (! w) {
        errno = ENOMEM;
        return (-1);
    }

    (void)MultiByteToWideChar(SAFE_STAT64_CODEPAGE, MB_ERR_INVALID_CHARS,
                              path, -1, w, wlen);

    rc = safe_stat64W(w, st);
    free(w);

    return rc;
}
#endif  /* _MSC_VER */


int
img2sixel_compat_stat(const char *path, struct stat *stat_buffer)
{
    char *buffer;
    char const *libc_path;
    int result;

    buffer = NULL;
    libc_path = NULL;
    result = (-1);

    if (path == NULL || stat_buffer == NULL) {
        errno = EINVAL;
        return (-1);
    }

    if (img2sixel_compat_prepare_path(path, &buffer, &libc_path) < 0) {
        return (-1);
    }

#if defined(_MSC_VER)
    result = safe_stat64A(libc_path, stat_buffer);
#else
    result = stat(libc_path, stat_buffer);
#endif

    if (buffer != NULL) {
        free(buffer);
    }

    return result;
}

int
img2sixel_compat_rename(const char *src_path, const char *dst_path)
{
    char *src_buffer;
    char const *libc_src;
    char *dst_buffer;
    char const *libc_dst;
    int result;

    src_buffer = NULL;
    libc_src = NULL;
    dst_buffer = NULL;
    libc_dst = NULL;
    result = (-1);

    if (src_path == NULL || dst_path == NULL) {
        errno = EINVAL;
        return (-1);
    }

    if (img2sixel_compat_prepare_path(src_path,
                                      &src_buffer,
                                      &libc_src) < 0) {
        return (-1);
    }
    if (img2sixel_compat_prepare_path(dst_path,
                                      &dst_buffer,
                                      &libc_dst) < 0) {
        if (src_buffer != NULL) {
            free(src_buffer);
        }
        return (-1);
    }

    /* rename is available on MSVC and avoids undefined _rename warnings. */
    result = rename(libc_src, libc_dst);

    if (src_buffer != NULL) {
        free(src_buffer);
    }
    if (dst_buffer != NULL) {
        free(dst_buffer);
    }

    return result;
}

ssize_t
img2sixel_compat_write(int fd, const void *buffer, size_t count)
{
#if defined(_MSC_VER)
    return (ssize_t)_write(fd, buffer, (unsigned int)count);
#else
    return write(fd, buffer, count);
#endif
}

void
img2sixel_compat_puts(const char *buf)
{
    img2sixel_compat_write(STDOUT_FILENO, buf, sizeof(buf));
}
