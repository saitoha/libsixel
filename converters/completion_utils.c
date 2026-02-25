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
#include "completion_utils.h"
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

#if defined(BUILD_IMG2SIXEL) && defined(HAVE_COMPLETION_EMBED_H)
#include "completion_embed.h"
#define IMG2SIXEL_HAVE_COMPLETION_EMBED 1
#elif defined(BUILD_IMG2SIXEL) && defined(__has_include)
#if __has_include("completion_embed.h")
#include "completion_embed.h"
#define IMG2SIXEL_HAVE_COMPLETION_EMBED 1
#endif
#endif

#define IMG2SIXEL_COMPLETION_MODE_FILE 0644
#define IMG2SIXEL_COMPLETION_DIR_MODE 0755
#define IMG2SIXEL_TMP_SUFFIX ".tmpXXXXXX"

#define IMG2SIXEL_COMPLETION_SHELL_BASH  1
#define IMG2SIXEL_COMPLETION_SHELL_ZSH   2

/* ------------------------------------------------------------------------ */
/* private copies of the compat helpers we consume                          */
/*                                                                          */
/* The completion binary must not reach into src/compat_stub.c because      */
/* amalgamation mode concatenates all sources into a single file.  To avoid */
/* clashing with the library's shim symbols we keep a tool-specific         */
/* prefix.                                                                  */
/* ------------------------------------------------------------------------ */

static char *img2sixel_compat_strerror(int error_number,
                                       char *buffer,
                                       size_t buffer_size);
static FILE *img2sixel_compat_fopen(const char *filename, const char *mode);
const char *img2sixel_compat_getenv(const char *name);
static int img2sixel_compat_prepare_path(char const *path,
                                         char **buffer_out,
                                         char const **libc_path_out);
#if defined(_MSC_VER)
static int img2sixel_compat_strcpy(char *destination,
                                   size_t destination_size,
                                   const char *source);
#endif
static int img2sixel_compat_chmod(const char *path, mode_t mode);
#if !defined(HAVE_MKSTEMP)
static int img2sixel_compat_mktemp(char *templ, size_t buffer_size);
static int img2sixel_compat_open(const char *path, int flags, ...);
#endif
static int img2sixel_compat_close(int fd);
static int img2sixel_compat_unlink(const char *path);
static int img2sixel_compat_access(const char *path, int mode);
static int img2sixel_compat_stat(const char *path, struct stat *stat_buffer);
static int img2sixel_compat_rename(const char *src_path,
                                   const char *dst_path);
static ssize_t img2sixel_compat_write(int fd,
                                      const void *buffer,
                                      size_t count);
static void img2sixel_compat_puts(const char *buf);
static void img2sixel_log_errno(const char *fmt, ...);
int img2sixel_trace_topic_is_enabled(const char *topic);
static void img2sixel_trace_topic_message(const char *topic,
                                          const char *format, ...);

static char *
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
static int
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

    buffer_size = img2sixel_path_to_libc_buffer_size(path);
    if (buffer_size > 0u) {
        buffer = (char *)malloc(buffer_size);
        if (buffer == NULL) {
            errno = ENOMEM;
            return (-1);
        }
        libc_path = img2sixel_path_to_libc(path, buffer, buffer_size);
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

static FILE *
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

/*
 * Use the platform-specific chmod entry point so MSVC does not warn
 * about the POSIX spelling.
 */
static int
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
        img2sixel_log_errno("chmod failed path=%s libc_path=%s",
                            path, libc_path);
        if (buffer != NULL) {
            free(buffer);
        }
        return (-1);
    }
#elif defined(HAVE_CHMOD)
    if (chmod(libc_path, mode) != 0) {
        img2sixel_log_errno("chmod failed path=%s libc_path=%s",
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
static int
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

static int
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

static int
img2sixel_compat_close(int fd)
{
#if defined(_MSC_VER)
    return _close(fd);
#else
    return close(fd);
#endif
}

static int
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

static int
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


static int
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

static int
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

static ssize_t
img2sixel_compat_write(int fd, const void *buffer, size_t count)
{
#if defined(_MSC_VER)
    return (ssize_t)_write(fd, buffer, (unsigned int)count);
#else
    return write(fd, buffer, count);
#endif
}

static void
img2sixel_compat_puts(const char *buf)
{
    img2sixel_compat_write(STDOUT_FILENO, buf, sizeof(buf));
}

/* ------------------------------------------------------------------------ */
/* helpers for platform abstractions */
/* ------------------------------------------------------------------------ */


/*
 * Return non-zero when SIXEL_TRACE_TOPIC contains the given token.
 * Supported separators are comma, colon, semicolon, and whitespace.
 */
int
img2sixel_trace_topic_is_enabled(const char *topic)
{
    const char *topics;
    const char *cursor;
    const char *token_end;
    size_t topic_length;
    size_t token_length;

    topics = NULL;
    cursor = NULL;
    token_end = NULL;
    topic_length = 0u;
    token_length = 0u;

    if (topic == NULL || topic[0] == '\0') {
        return 0;
    }

    topic_length = strlen(topic);
    if (topic_length == 0u) {
        return 0;
    }

    topics = img2sixel_compat_getenv("SIXEL_TRACE_TOPIC");
    if (topics == NULL || topics[0] == '\0') {
        return 0;
    }

    cursor = topics;
    while (*cursor != '\0') {
        while (*cursor != '\0' &&
               (*cursor == ' ' || *cursor == '\t' || *cursor == ',' ||
                *cursor == ':' || *cursor == ';')) {
            ++cursor;
        }
        if (*cursor == '\0') {
            break;
        }

        token_end = cursor;
        while (*token_end != '\0' &&
               *token_end != ' ' && *token_end != '\t' &&
               *token_end != ',' && *token_end != ':' &&
               *token_end != ';') {
            ++token_end;
        }

        token_length = (size_t)(token_end - cursor);
        if (token_length == topic_length &&
                strncmp(cursor, topic, token_length) == 0) {
            return 1;
        }

        cursor = token_end;
    }

    return 0;
}

static void
img2sixel_trace_topic_message(const char *topic, const char *format, ...)
{
    va_list args;

    if (!img2sixel_trace_topic_is_enabled(topic)) {
        return;
    }

    fprintf(stderr,
            "img2sixel[%s]: ",
            topic != NULL && topic[0] != '\0' ? topic : "trace");

    {
        char message[1024];
        int written;

        message[0] = '\0';
        va_start(args, format);
#if HAVE_DIAGNOSTIC_FORMAT_NONLITERAL
# if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wformat-nonliteral"
# elif defined(__GNUC__) && !defined(__PCC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wformat-nonliteral"
# endif
#endif
        written = vsnprintf(message, sizeof(message), format, args);
#if HAVE_DIAGNOSTIC_FORMAT_NONLITERAL
# if defined(__clang__)
#  pragma clang diagnostic pop
# elif defined(__GNUC__) && !defined(__PCC__)
#  pragma GCC diagnostic pop
# endif
#endif
        va_end(args);

        if (written < 0) {
            fputs("trace formatting failed", stderr);
        } else if ((size_t)written >= sizeof(message)) {
            fputs(message, stderr);
            fputs("...", stderr);
        } else {
            fputs(message, stderr);
        }
    }

    fprintf(stderr, "\n");
}

static int
img2sixel_fsync(int fd)
{
    int sync_result;

    sync_result = 0;
    img2sixel_trace_topic_message("lifecycle",
                                  "fsync begin: fd=%d",
                                  fd);
#if defined(__EMSCRIPTEN__)
    /*
     * Emscripten's Node.js backend can expose stdout/stderr streams without
     * stream.node metadata when the descriptor is redirected to a pipe.
     * Calling fsync() in that case raises a JavaScript TypeError during test
     * execution, even though completion output has already been written.
     *
     * Keep completion output deterministic by treating fsync as a no-op on
     * emscripten targets.
     */
    (void)fd;
    sync_result = 0;
#elif HAVE__COMMIT
    sync_result = _commit(fd);
#elif HAVE_FSYNC
    sync_result = fsync(fd);
#else
    /* Keep an explicit no-op for environments without fsync support. */
    (void)fd;
    sync_result = 0;
#endif
    img2sixel_trace_topic_message("lifecycle",
                                  "fsync end: fd=%d result=%d errno=%d",
                                  fd,
                                  sync_result,
                                  errno);
    return sync_result;
}

#if HAVE__MKDIR
int _mkdir (const char *);
#endif

static int
img2sixel_mkdir(const char *path, mode_t mode)
{
    char *buffer;
    char const *libc_path;

    buffer = NULL;
    libc_path = NULL;

    if (img2sixel_compat_prepare_path(path, &buffer, &libc_path) < 0) {
        return (-1);
    }

#if HAVE__MKDIR
    (void)mode;
    if (_mkdir(libc_path) != 0) {
        img2sixel_log_errno("mkdir failed path=%s libc_path=%s",
                            path, libc_path);
        if (buffer != NULL) {
            free(buffer);
        }
        return (-1);
    }
#elif HAVE_MKDIR
    if (mkdir(libc_path, mode) != 0) {
        img2sixel_log_errno("mkdir failed path=%s libc_path=%s",
                            path, libc_path);
        if (buffer != NULL) {
            free(buffer);
        }
        return (-1);
    }
#else
    /* Silence unused arguments when mkdir is unavailable. */
    (void)path;
    (void)mode;
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

static void
img2sixel_log_errno(const char *fmt, ...)
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

static int
img2sixel_shell_mask(const char *value)
{
    int mask;

    mask = 0;
    if (value == NULL || value[0] == '\0' || strcmp(value, "all") == 0) {
        mask = IMG2SIXEL_COMPLETION_SHELL_BASH
            | IMG2SIXEL_COMPLETION_SHELL_ZSH;
    } else if (strcmp(value, "bash") == 0) {
        mask = IMG2SIXEL_COMPLETION_SHELL_BASH;
    } else if (strcmp(value, "zsh") == 0) {
        mask = IMG2SIXEL_COMPLETION_SHELL_ZSH;
    }

    return mask;
}

int
read_entire_file(const char *path, char **buf, size_t *len)
{
    FILE *fp;
    struct stat st;
    size_t read_len;
    char *tmp;

    if (buf == NULL || len == NULL || path == NULL) {
        errno = EINVAL;
        return -1;
    }

    *buf = NULL;
    *len = 0;

    if (img2sixel_compat_stat(path, &st) != 0) {
        return (-1);
    }

    if (st.st_size <= 0) {
        errno = EINVAL;
        return (-1);
    }

    fp = img2sixel_compat_fopen(path, "rb");
    if (fp == NULL) {
        return (-1);
    }

    tmp = (char *)malloc((size_t)st.st_size + 1);
    if (tmp == NULL) {
        fclose(fp);
        return (-1);
    }

    read_len = fread(tmp, 1, (size_t)st.st_size, fp);
    if (read_len != (size_t)st.st_size) {
        free(tmp);
        fclose(fp);
        errno = EIO;
        return (-1);
    }
    if (fclose(fp) != 0) {
        free(tmp);
        return (-1);
    }

    if (read_len <= 0) {
        return (-1);
    }

    tmp[st.st_size] = '\0';
    *buf = tmp;
    *len = (size_t)st.st_size;

    return 0;
}

int
write_atomic(const char *dst_path, const void *buf, size_t len, mode_t mode)
{
    int fd;
    ssize_t written;
    size_t total;
    char *tmp_path;
    size_t dst_len;
    size_t suffix_len;
    int saved_errno;

    if (dst_path == NULL || buf == NULL) {
        errno = EINVAL;
        return -1;
    }

    dst_len = strlen(dst_path);
    suffix_len = strlen(IMG2SIXEL_TMP_SUFFIX);
    tmp_path = (char *)malloc(dst_len + suffix_len + 1);
    if (tmp_path == NULL) {
        return -1;
    }

    memcpy(tmp_path, dst_path, dst_len);
    memcpy(tmp_path + dst_len, IMG2SIXEL_TMP_SUFFIX, suffix_len + 1);

#if defined(HAVE_MKSTEMP)
    fd = mkstemp(tmp_path);
    if (fd < 0) {
        free(tmp_path);
        return -1;
    }
#else
    if (img2sixel_compat_mktemp(tmp_path, dst_len + suffix_len + 1) != 0) {
        free(tmp_path);
        return -1;
    }
    fd = img2sixel_compat_open(tmp_path,
                           O_RDWR | O_CREAT | O_TRUNC,
                           S_IRUSR | S_IWUSR);
    if (fd < 0) {
        saved_errno = errno;
        free(tmp_path);
        errno = saved_errno;
        return -1;
    }
#endif

#if !defined(_WIN32) && HAVE_FCHMOD
    if (fchmod(fd, mode) != 0) {
        int saved_errno_fchmod;

        saved_errno_fchmod = errno;
        (void)img2sixel_compat_close(fd);
        (void)img2sixel_compat_unlink(tmp_path);
        free(tmp_path);
        errno = saved_errno_fchmod;
        return -1;
    }
#else
    (void)mode;
#endif

    total = 0;
    while (total < len) {
        written = img2sixel_compat_write(fd,
                                         (const char *)buf + total,
                                         len - total);
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            saved_errno = errno;
            (void)img2sixel_compat_close(fd);
            (void)img2sixel_compat_unlink(tmp_path);
            free(tmp_path);
            errno = saved_errno;
            return -1;
        }
        total += (size_t)written;
    }

    if (img2sixel_fsync(fd) != 0) {
        int saved_errno_fsync;

        saved_errno_fsync = errno;
        if (saved_errno_fsync == ENOSYS) {
            /* Treat missing fsync support as a best-effort success. */
            saved_errno_fsync = 0;
        }
        if (saved_errno_fsync == 0) {
            if (img2sixel_compat_close(fd) != 0) {
                saved_errno = errno;
                (void)img2sixel_compat_unlink(tmp_path);
                free(tmp_path);
                errno = saved_errno;
                return -1;
            }
            goto rename_stage;
        }
        (void)img2sixel_compat_close(fd);
        (void)img2sixel_compat_unlink(tmp_path);
        free(tmp_path);
        errno = saved_errno_fsync;
        return -1;
    }

    if (img2sixel_compat_close(fd) != 0) {
        saved_errno = errno;
        (void)img2sixel_compat_unlink(tmp_path);
        free(tmp_path);
        errno = saved_errno;
        return -1;
    }

rename_stage:
    if (img2sixel_compat_rename(tmp_path, dst_path) != 0) {
        saved_errno = errno;
        (void)img2sixel_compat_unlink(tmp_path);
        free(tmp_path);
        errno = saved_errno;
        return -1;
    }

    free(tmp_path);
    return 0;
}

static int
img2sixel_is_drive_root_path(const char *path)
{
    size_t len;
    int is_letter;

    if (path == NULL) {
        return 0;
    }

    len = strlen(path);
    if (len == 0u) {
        return 0;
    }

    /* Match /cygdrive/<letter> and /cygdrive/<letter>/. */
    if ((len == 11u || len == 12u)
            && strncmp(path, "/cygdrive/", 10) == 0) {
        is_letter = ((path[10] >= 'A' && path[10] <= 'Z')
                     || (path[10] >= 'a' && path[10] <= 'z'));
        if (is_letter != 0 && (len == 11u || path[11] == '/')) {
            return 1;
        }
    }

    /* Match /<letter> and /<letter>/. */
    if ((len == 2u || len == 3u) && path[0] == '/') {
        is_letter = ((path[1] >= 'A' && path[1] <= 'Z')
                     || (path[1] >= 'a' && path[1] <= 'z'));
        if (is_letter != 0 && (len == 2u || path[2] == '/')) {
            return 1;
        }
    }

    /* Match <letter>:, <letter>:/ and <letter>:\\ . */
    if ((len == 2u || len == 3u) && path[1] == ':') {
        is_letter = ((path[0] >= 'A' && path[0] <= 'Z')
                     || (path[0] >= 'a' && path[0] <= 'z'));
        if (is_letter != 0
                && (len == 2u || path[2] == '/' || path[2] == '\\')) {
            return 1;
        }
    }

    return 0;
}

int
ensure_dir_p(const char *path, mode_t mode)
{
    size_t len;
    char *tmp;
    size_t i;
    char *component;
    size_t component_length;

    if (path == NULL) {
        errno = EINVAL;
        return -1;
    }

    len = strlen(path);
    tmp = (char *)malloc(len + 1);
    if (tmp == NULL) {
        return -1;
    }

    memcpy(tmp, path, len + 1);
    component = NULL;
    component_length = 0u;

    for (i = 1; i < len; ++i) {
        if (tmp[i] == '/') {
            char saved;

            saved = tmp[i];
            tmp[i] = '\0';
            if (tmp[0] != '\0') {
                /*
                 * Drive-qualified paths include a `letter:` prefix.  Keep
                 * the drive component intact so we never attempt to
                 * `mkdir("d:")` even when cosmopolitan builds do not define
                 * _WIN32. The ladder below sketches the intent:
                 *
                 *   d:/logs/run
                 *   |  |
                 *   |  +-- skip the root when `i == 2`
                 *   +----- drive column we should preserve as-is
                 */
                if (i == 2
                        && ((tmp[0] >= 'A' && tmp[0] <= 'Z')
                            || (tmp[0] >= 'a' && tmp[0] <= 'z'))
                        && tmp[1] == ':') {
                    tmp[i] = saved;
                    continue;
                }
                /*
                 * Treat drive roots as immutable mount points.  We skip
                 * /cygdrive/<letter>, /<letter>, and <letter>: forms so the
                 * step-wise mkdir loop never targets pseudo roots.
                 */
                if (img2sixel_is_drive_root_path(tmp) != 0) {
                    tmp[i] = saved;
                    continue;
                }
                component = strrchr(tmp, '/');
                if (component == NULL) {
                    component = tmp;
                } else {
                    component += 1;
                }
                component_length = strlen(component);
                /*
                 * Skip requests for "." and ".." so Windows builds avoid
                 * creating a literal "directory dot" segment.  The drawing
                 * below sketches how the guard trims the problematic edge:
                 *
                 *     converters/./tmp
                 *     |---------| |--|
                 *          |       +---- ignored because the component is
                 *          |             "tmp"
                 *          +------------ ignored because the component is
                 *                        "."
                 */
                if (!((component_length == 1u && component[0] == '.')
                        || (component_length == 2u
                            && component[0] == '.'
                            && component[1] == '.'))) {
                    if (img2sixel_mkdir(tmp, mode) != 0) {
                        if (errno != EEXIST) {
                            int saved_errno;

                            img2sixel_log_errno(
                                "ensure_dir_p mkdir stage failed path=%s",
                                tmp);
                            saved_errno = errno;
                            free(tmp);
                            errno = saved_errno;
                            return -1;
                        }
                    } else {
                        /*
                         * Apply the requested permissions explicitly so the
                         * directory remains writable even when the caller
                         * inherits a restrictive umask.
                         */
                        if (img2sixel_compat_chmod(tmp, mode) != 0) {
                            int saved_errno;

                            img2sixel_log_errno(
                                "ensure_dir_p chmod stage failed path=%s",
                                tmp);
                            saved_errno = errno;
                            free(tmp);
                            errno = saved_errno;
                            return -1;
                        }
                    }
                }
            }
            tmp[i] = saved;
        }
    }

    if (img2sixel_is_drive_root_path(tmp) != 0) {
        free(tmp);
        return 0;
    }

    if (img2sixel_mkdir(tmp, mode) != 0) {
        if (errno != EEXIST) {
            int saved_errno;

            img2sixel_log_errno(
                "ensure_dir_p final mkdir stage failed path=%s", tmp);
            saved_errno = errno;
            free(tmp);
            errno = saved_errno;
            return -1;
        }
    } else {
        if (img2sixel_compat_chmod(tmp, mode) != 0) {
            int saved_errno;

            img2sixel_log_errno(
                "ensure_dir_p final chmod stage failed path=%s", tmp);
            saved_errno = errno;
            free(tmp);
            errno = saved_errno;
            return -1;
        }
    }

    free(tmp);
    return 0;
}

int
files_equal(const char *path, const void *buf, size_t len)
{
    FILE *fp;
    size_t read_len;
    unsigned char file_buf[4096];

    if (path == NULL || buf == NULL) {
        errno = EINVAL;
        return -1;
    }

    fp = img2sixel_compat_fopen(path, "rb");
    if (fp == NULL) {
        if (errno == ENOENT) {
            return 0;
        }
        return -1;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }

    {
        long size_long;

        size_long = ftell(fp);
        if (size_long < 0) {
            fclose(fp);
            return -1;
        }
        if ((size_t)size_long != len) {
            fclose(fp);
            return 0;
        }
    }

    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return -1;
    }

    while (len > 0) {
        size_t to_read;

        to_read = sizeof(file_buf);
        if (to_read > len) {
            to_read = len;
        }
        read_len = fread(file_buf, 1, to_read, fp);
        if (read_len != to_read) {
            fclose(fp);
            return -1;
        }
        if (memcmp(file_buf, (const unsigned char *)buf, to_read) != 0) {
            fclose(fp);
            return 0;
        }
        buf = (const unsigned char *)buf + to_read;
        len -= to_read;
    }

    if (fclose(fp) != 0) {
        return -1;
    }

    return 1;
}

int
ensure_line_in_file(const char *path, const char *line)
{
    char *content;
    size_t len;
    size_t pos;
    size_t line_len;
    int found;
    FILE *fp;
    int appended;

    if (path == NULL || line == NULL) {
        errno = EINVAL;
        return -1;
    }

    content = NULL;
    len = 0;
    found = 0;
    appended = 0;
    line_len = strlen(line);

    if (read_entire_file(path, &content, &len) == 0) {
        pos = 0;
        while (pos <= len) {
            size_t start;
            size_t segment_len;

            start = pos;
            while (pos < len && content[pos] != '\n') {
                ++pos;
            }
            segment_len = pos - start;
            if (segment_len == line_len) {
                if (memcmp(content + start, line, line_len) == 0) {
                    found = 1;
                    break;
                }
            }
            if (pos < len && content[pos] == '\n') {
                ++pos;
            } else {
                break;
            }
        }
    } else if (errno != ENOENT) {
        return -1;
    }

    if (found) {
        free(content);
        return 0;
    }

    fp = img2sixel_compat_fopen(path, "ab");
    if (fp == NULL) {
        free(content);
        return -1;
    }

    if (content != NULL && len > 0 && content[len - 1] != '\n') {
        if (fwrite("\n", 1, 1, fp) != 1) {
            fclose(fp);
            free(content);
            return -1;
        }
    }

    if (fwrite(line, 1, line_len, fp) != line_len) {
        fclose(fp);
        free(content);
        return -1;
    }

    if (fwrite("\n", 1, 1, fp) != 1) {
        fclose(fp);
        free(content);
        return -1;
    }

    if (fclose(fp) != 0) {
        free(content);
        return -1;
    }

    free(content);
    appended = 1;

    return appended;
}

static int
img2sixel_join_path(const char *base, const char *suffix, char **out)
{
    size_t base_len;
    size_t suffix_len;
    char *tmp;

    if (base == NULL || suffix == NULL || out == NULL) {
        errno = EINVAL;
        return -1;
    }

    base_len = strlen(base);
    suffix_len = strlen(suffix);
    tmp = (char *)malloc(base_len + suffix_len + 1);
    if (tmp == NULL) {
        return -1;
    }

    memcpy(tmp, base, base_len);
    memcpy(tmp + base_len, suffix, suffix_len + 1);
    *out = tmp;

    return 0;
}

static int
img2sixel_try_embed(const char *shell, char **out, size_t *len)
{
#if defined(IMG2SIXEL_HAVE_COMPLETION_EMBED)
    const unsigned char *data;
    size_t data_len;

    data = NULL;
    data_len = 0;

    if (strcmp(shell, "bash") == 0) {
        data_len = sizeof(img2sixel_bash_completion) - 1;
        data = (unsigned char *)img2sixel_bash_completion;
    } else if (strcmp(shell, "zsh") == 0) {
        data_len = sizeof(img2sixel_zsh_completion) - 1;
        data = (unsigned char *)img2sixel_zsh_completion;
    }

    if (data == NULL) {
        return -1;
    }

    *out = (char *)malloc(data_len + 1);
    if (*out == NULL) {
        return -1;
    }

    memcpy(*out, data, data_len + 1);
    *len = data_len;
    return 0;
#else
    (void)shell;
    (void)out;
    (void)len;
    return -1;
#endif
}

int
get_completion_text(const char *shell, char **out, size_t *len)
{
    if (shell == NULL || out == NULL || len == NULL) {
        errno = EINVAL;
        return -1;
    }

    *out = NULL;
    *len = 0;

    if (img2sixel_try_embed(shell, out, len) == 0) {
        return 0;
    }

    errno = ENOENT;
    return -1;
}

static const char *
img2sixel_completion_home(void)
{
    const char *home;

    home = img2sixel_compat_getenv("IMG2SIXEL_COMPLETION_HOME");
    if (home != NULL && home[0] != '\0') {
        return home;
    }

    home = img2sixel_compat_getenv("HOME");
    if (home == NULL || home[0] == '\0') {
        return NULL;
    }

    return home;
}

static int
img2sixel_parse_bash_major(const char *version)
{
    int i;
    int major;
    char ch;

    if (version == NULL || version[0] == '\0') {
        return -1;
    }

    major = 0;
    for (i = 0; version[i] != '\0'; ++i) {
        ch = version[i];
        if (ch == '.') {
            if (i == 0) {
                return -1;
            }
            return major;
        }
        if (ch < '0' || ch > '9') {
            return -1;
        }
        major = (major * 10) + (int)(ch - '0');
    }

    if (i == 0) {
        return -1;
    }

    return major;
}

static int
img2sixel_prefer_legacy_bash_path(void)
{
    const char *version;
    const char *override_version;
    int major;

    /* ------------------------------------------------------------------ */
    /*                                                                    */
    /*   BASH_VERSION                                                     */
    /*       |                                                            */
    /*       v                                                            */
    /*   +-----------+ yes +-------------------------------+              */
    /*   | missing?  |---->| keep modern XDG directory     |              */
    /*   +-----------+     +-------------------------------+              */
    /*       | no                                                         */
    /*       v                                                            */
    /*   +--------------------+     +-----------------------+             */
    /*   |                    | yes | prefer legacy ~/.bash_|             */
    /*   | major < 4 detected |---->| completion.d location |             */
    /*   |                    |     |  (create if required) |             */
    /*   +--------------------+     +-----------------------|             */
    /*       | no                                                         */
    /*       v                                                            */
    /*   +----------------------------+                                   */
    /*   | stick with XDG default dir |                                   */
    /*   +----------------------------+                                   */
    /*                                                                    */
    /* ------------------------------------------------------------------ */
    /*
     * Test shells such as bash may expose a read-only BASH_VERSION
     * variable that cannot be overridden from the environment. Prefer an
     * explicit IMG2SIXEL_BASH_VERSION_OVERRIDE knob so tests can request
     * legacy completion behavior deterministically.
     */
    override_version = img2sixel_compat_getenv(
        "IMG2SIXEL_BASH_VERSION_OVERRIDE");
    if (override_version != NULL && override_version[0] != '\0') {
        version = override_version;
    } else {
        version = img2sixel_compat_getenv("BASH_VERSION");
    }
    if (version == NULL) {
        return 0;
    }

    major = img2sixel_parse_bash_major(version);
    if (major < 0) {
        return 0;
    }
    if (major < 4) {
        return 1;
    }

    return 0;
}

static int
img2sixel_install_single(const char *shell, const char *target_path,
                         const char *target_dir, const char *fallback_path,
                         const void *buf, size_t len)
{
    int equal;
    int ret;

    if (ensure_dir_p(target_dir, IMG2SIXEL_COMPLETION_DIR_MODE) != 0) {
        img2sixel_log_errno("failed to create directory %s", target_dir);
        return -1;
    }

    equal = files_equal(target_path, buf, len);
    if (equal == 0) {
        ret = write_atomic(target_path, buf, len,
                           IMG2SIXEL_COMPLETION_MODE_FILE);
        if (ret != 0) {
            img2sixel_log_errno("failed to write %s", target_path);
            return -1;
        }
        printf("updated %s\n", target_path);
    } else if (equal == 1) {
        printf("unchanged %s\n", target_path);
    } else {
        img2sixel_log_errno("failed to compare %s", target_path);
        return -1;
    }

    if (fallback_path != NULL
        && img2sixel_compat_access(fallback_path, F_OK) == 0) {
        equal = files_equal(fallback_path, buf, len);
        if (equal == 0) {
            ret = write_atomic(fallback_path, buf, len,
                               IMG2SIXEL_COMPLETION_MODE_FILE);
            if (ret != 0) {
                img2sixel_log_errno("failed to write %s", fallback_path);
                return -1;
            }
            printf("updated %s\n", fallback_path);
        } else if (equal == 1) {
            printf("unchanged %s\n", fallback_path);
        } else {
            img2sixel_log_errno("failed to compare %s", fallback_path);
            return -1;
        }
    }

    (void)shell;
    return 0;
}

static int
img2sixel_install_zsh_rc(const char *home)
{
    char *rc_path;
    int appended;

    if (img2sixel_join_path(home, "/.zshrc", &rc_path) != 0) {
        return -1;
    }

    appended = ensure_line_in_file(rc_path, "fpath+=(\"$HOME/.zfunc\")");
    if (appended < 0) {
        img2sixel_log_errno("failed to update %s", rc_path);
        free(rc_path);
        return -1;
    }
    if (appended > 0) {
        printf("appended %s\n", rc_path);
    }

    appended = ensure_line_in_file(rc_path,
        "autoload -Uz compinit && compinit -u");
    if (appended < 0) {
        img2sixel_log_errno("failed to update %s", rc_path);
        free(rc_path);
        return -1;
    }
    if (appended > 0) {
        printf("appended %s\n", rc_path);
    }

    free(rc_path);
    return 0;
}

static int
img2sixel_handle_install(int mask)
{
    const char *home;
    char *target_path;
    char *target_dir;
    char *fallback_path;
    char *fallback_dir;
    char *buf;
    size_t len;
    int prefer_legacy;

    home = img2sixel_completion_home();
    if (home == NULL) {
        fprintf(stderr, "HOME is not set; cannot install completions\n");
        return -1;
    }

    if ((mask & IMG2SIXEL_COMPLETION_SHELL_BASH) != 0) {
        if (get_completion_text("bash", &buf, &len) != 0) {
            img2sixel_log_errno("failed to load bash completion data");
            return -1;
        }

        prefer_legacy = img2sixel_prefer_legacy_bash_path();
        if (img2sixel_join_path(home,
            "/.local/share/bash-completion/completions/img2sixel",
            &target_path) != 0) {
            free(buf);
            return -1;
        }

        if (img2sixel_join_path(home,
            "/.local/share/bash-completion/completions", &target_dir) != 0) {
            free(buf);
            free(target_path);
            return -1;
        }

        if (img2sixel_join_path(home,
            "/.bash_completion.d/img2sixel", &fallback_path) != 0) {
            free(buf);
            free(target_path);
            free(target_dir);
            return -1;
        }

        fallback_dir = NULL;
        if (prefer_legacy != 0) {
            if (img2sixel_join_path(home, "/.bash_completion.d",
                &fallback_dir) != 0) {
                free(buf);
                free(target_path);
                free(target_dir);
                free(fallback_path);
                return -1;
            }
            if (img2sixel_install_single("bash", fallback_path, fallback_dir,
                    target_path, buf, len) != 0) {
                free(buf);
                free(target_path);
                free(target_dir);
                free(fallback_path);
                free(fallback_dir);
                return -1;
            }
        } else {
            if (img2sixel_install_single("bash", target_path, target_dir,
                    fallback_path, buf, len) != 0) {
                free(buf);
                free(target_path);
                free(target_dir);
                free(fallback_path);
                return -1;
            }
        }

        free(buf);
        free(target_path);
        free(target_dir);
        free(fallback_path);
        if (fallback_dir != NULL) {
            free(fallback_dir);
        }
    }

    if ((mask & IMG2SIXEL_COMPLETION_SHELL_ZSH) != 0) {
        if (get_completion_text("zsh", &buf, &len) != 0) {
            img2sixel_log_errno("failed to load zsh completion data");
            return -1;
        }

        if (img2sixel_join_path(home, "/.zfunc/_img2sixel",
                                &target_path) != 0) {
            free(buf);
            return -1;
        }

        if (img2sixel_join_path(home, "/.zfunc", &target_dir) != 0) {
            free(buf);
            free(target_path);
            return -1;
        }

        if (img2sixel_install_single("zsh", target_path, target_dir,
                                      NULL, buf, len) != 0) {
            free(buf);
            free(target_path);
            free(target_dir);
            return -1;
        }

        free(buf);
        free(target_path);
        free(target_dir);

        if (img2sixel_install_zsh_rc(home) != 0) {
            return -1;
        }
    }

    return 0;
}

static int
img2sixel_handle_show(int mask)
{
    char *buf;
    size_t len;

    if ((mask & IMG2SIXEL_COMPLETION_SHELL_BASH) != 0
        && (mask & IMG2SIXEL_COMPLETION_SHELL_ZSH) != 0) {
        if (get_completion_text("bash", &buf, &len) != 0) {
            img2sixel_log_errno("failed to load bash completion data");
            return -1;
        }
        img2sixel_compat_puts("# ---- bash ----\n");
        (void)img2sixel_compat_write(STDOUT_FILENO, buf, len);
        if (len == 0 || buf[len - 1] != '\n') {
            printf("\n");
        }
        free(buf);

        if (get_completion_text("zsh", &buf, &len) != 0) {
            img2sixel_log_errno("failed to load zsh completion data");
            return -1;
        }
        img2sixel_compat_puts("# ---- zsh ----\n");
        (void)img2sixel_compat_write(STDOUT_FILENO, buf, len);
        if (len == 0 || buf[len - 1] != '\n') {
            printf("\n");
        }
        free(buf);

        return 0;
    }

    if ((mask & IMG2SIXEL_COMPLETION_SHELL_BASH) != 0) {
        if (get_completion_text("bash", &buf, &len) != 0) {
            img2sixel_log_errno("failed to load bash completion data");
            return -1;
        }
        (void)img2sixel_compat_write(STDOUT_FILENO, buf, len);
        if (len == 0 || buf[len - 1] != '\n') {
            printf("\n");
        }
        free(buf);
    }

    if ((mask & IMG2SIXEL_COMPLETION_SHELL_ZSH) != 0) {
        if (get_completion_text("zsh", &buf, &len) != 0) {
            img2sixel_log_errno("failed to load zsh completion data");
            return -1;
        }
        (void)img2sixel_compat_write(STDOUT_FILENO, buf, len);
        if (len == 0 || buf[len - 1] != '\n') {
            printf("\n");
        }
        free(buf);
    }

    (void) img2sixel_fsync(STDOUT_FILENO);

    return 0;
}

static int
img2sixel_unlink_result(const char *path)
{
    if (img2sixel_compat_unlink(path) == 0) {
        printf("removed %s\n", path);
        return 0;
    }
    if (errno == ENOENT) {
        printf("missing %s\n", path);
        return 0;
    }
    img2sixel_log_errno("failed to remove %s", path);
    return -1;
}

static int
img2sixel_handle_uninstall(int mask)
{
    const char *home;
    char *path;
    int ret;

    home = img2sixel_completion_home();
    if (home == NULL) {
        fprintf(stderr, "HOME is not set; cannot uninstall completions\n");
        return -1;
    }

    if ((mask & IMG2SIXEL_COMPLETION_SHELL_BASH) != 0) {
        if (img2sixel_join_path(home,
            "/.local/share/bash-completion/completions/img2sixel",
            &path) != 0) {
            return -1;
        }
        ret = img2sixel_unlink_result(path);
        free(path);
        if (ret != 0) {
            return -1;
        }

        if (img2sixel_join_path(home,
            "/.bash_completion.d/img2sixel", &path) != 0) {
            return -1;
        }
        ret = img2sixel_unlink_result(path);
        free(path);
        if (ret != 0) {
            return -1;
        }
    }

    if ((mask & IMG2SIXEL_COMPLETION_SHELL_ZSH) != 0) {
        if (img2sixel_join_path(home, "/.zfunc/_img2sixel", &path) != 0) {
            return -1;
        }
        ret = img2sixel_unlink_result(path);
        free(path);
        if (ret != 0) {
            return -1;
        }
    }

    return 0;
}

static int
img2sixel_parse_completion(int argc, char **argv, int *mask,
                           const char **action)
{
    int i;
    int parsed_mask;
    const char *parsed_action;
    const char *arg;
    const char *value;
    int consumed_next;
    const char *option_name;
    const char *candidate_action;

    if (mask == NULL || action == NULL) {
        return 0;
    }

    parsed_mask = 0;
    parsed_action = NULL;
    arg = NULL;
    value = NULL;
    consumed_next = 0;
    option_name = NULL;
    candidate_action = NULL;

    for (i = 1; i < argc; ++i) {
        arg = argv[i];
        value = NULL;
        consumed_next = 0;
        option_name = NULL;
        candidate_action = NULL;

        if (strcmp(arg, "--") == 0) {
            break;
        }

        if (strncmp(arg, "--show-completion", 18) == 0) {
            candidate_action = "show";
            option_name = "--show-completion";
            if (arg[18] == '\0') {
                if (i + 1 < argc && argv[i + 1][0] != '-') {
                    value = argv[i + 1];
                    consumed_next = 1;
                }
            } else if (arg[18] == '=') {
                value = arg + 19;
            } else {
                continue;
            }
        } else if (strncmp(arg, "--install-completion", 21) == 0) {
            candidate_action = "install";
            option_name = "--install-completion";
            if (arg[21] == '\0') {
                if (i + 1 < argc && argv[i + 1][0] != '-') {
                    value = argv[i + 1];
                    consumed_next = 1;
                }
            } else if (arg[21] == '=') {
                value = arg + 22;
            } else {
                continue;
            }
        } else if (strncmp(arg, "--uninstall-completion", 23) == 0) {
            candidate_action = "uninstall";
            option_name = "--uninstall-completion";
            if (arg[23] == '\0') {
                if (i + 1 < argc && argv[i + 1][0] != '-') {
                    value = argv[i + 1];
                    consumed_next = 1;
                }
            } else if (arg[23] == '=') {
                value = arg + 24;
            } else {
                continue;
            }
        } else if (strncmp(arg, "-1", 2) == 0) {
            candidate_action = "show";
            option_name = "-1";
            if (arg[2] == '\0') {
                if (i + 1 < argc && argv[i + 1][0] != '-') {
                    value = argv[i + 1];
                    consumed_next = 1;
                }
            } else if (arg[2] == '=') {
                value = arg + 3;
            } else {
                value = arg + 2;
            }
        } else if (strncmp(arg, "-2", 2) == 0) {
            candidate_action = "install";
            option_name = "-2";
            if (arg[2] == '\0') {
                if (i + 1 < argc && argv[i + 1][0] != '-') {
                    value = argv[i + 1];
                    consumed_next = 1;
                }
            } else if (arg[2] == '=') {
                value = arg + 3;
            } else {
                value = arg + 2;
            }
        } else if (strncmp(arg, "-3", 2) == 0) {
            candidate_action = "uninstall";
            option_name = "-3";
            if (arg[2] == '\0') {
                if (i + 1 < argc && argv[i + 1][0] != '-') {
                    value = argv[i + 1];
                    consumed_next = 1;
                }
            } else if (arg[2] == '=') {
                value = arg + 3;
            } else {
                value = arg + 2;
            }
        } else {
            continue;
        }

        if (value == NULL || value[0] == '\0') {
            if (option_name == NULL) {
                option_name = "completion option";
            }
            fprintf(stderr, "missing completion target for %s\n",
                    option_name);
            return -1;
        }

        parsed_mask = img2sixel_shell_mask(value);
        if (parsed_mask == 0) {
            if (option_name == NULL) {
                option_name = "completion option";
            }
            fprintf(stderr,
                    "invalid completion target for %s: %s\n",
                    option_name, value);
            return -1;
        }

        if (consumed_next) {
            ++i;
        }

        parsed_action = candidate_action;
        *mask = parsed_mask;
        *action = parsed_action;
        return 1;
    }

    return 0;
}

int
img2sixel_handle_completion_cli(int argc, char **argv, int *exit_code)
{
    const char *action;
    int mask;
    int parsed;

    if (exit_code == NULL) {
        return -1;
    }

    action = NULL;
    mask = 0;
    parsed = img2sixel_parse_completion(argc, argv, &mask, &action);
    if (parsed <= 0) {
        if (parsed < 0) {
            *exit_code = EXIT_FAILURE;
            return -1;
        }
        *exit_code = 0;
        return 0;
    }

    if (strncmp(action, "show", 5) == 0) {
        if (img2sixel_handle_show(mask) != 0) {
            *exit_code = EXIT_FAILURE;
            return -1;
        }
        *exit_code = EXIT_SUCCESS;
        return 1;
    }

    if (strncmp(action, "install", 8) == 0) {
        if (img2sixel_handle_install(mask) != 0) {
            *exit_code = EXIT_FAILURE;
            return -1;
        }
        *exit_code = EXIT_SUCCESS;
        return 1;
    }

    if (strncmp(action, "uninstall", 10) == 0) {
        if (img2sixel_handle_uninstall(mask) != 0) {
            *exit_code = EXIT_FAILURE;
            return -1;
        }
        *exit_code = EXIT_SUCCESS;
        return 1;
    }

    /*
     * The parser only emits show/install/uninstall actions. Keep a
     * conservative success fallback for defensive forward compatibility.
     */
    *exit_code = EXIT_SUCCESS;
    return 1;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
