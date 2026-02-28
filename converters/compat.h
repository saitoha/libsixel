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

#ifndef IMG2SIXEL_COMPAT_H
#define IMG2SIXEL_COMPAT_H

#if HAVE_STDDEF_H
# include <stddef.h>
#endif
#if HAVE_STDIO_H
# include <stdio.h>
#endif
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif

#if _MSC_VER
# if !defined(_MODE_T_DEFINED)
typedef int mode_t;
#  define _MODE_T_DEFINED
# endif
#endif

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

/* Replicate POSIX access() flags for readability. */
#if !defined(R_OK)
# define R_OK 4
#endif
#if !defined(F_OK)
# define F_OK 0
#endif

#if defined(_MSC_VER) && !defined(_SSIZE_T_DEFINED)
# include <BaseTsd.h>
typedef SSIZE_T ssize_t;
# define _SSIZE_T_DEFINED
#endif

char *img2sixel_compat_strerror(int error_number,
                                char *buffer,
                                size_t buffer_size);
FILE *img2sixel_compat_fopen(const char *filename, const char *mode);
const char *img2sixel_compat_getenv(const char *name);
int img2sixel_compat_setenv(const char *name, const char *value);
int img2sixel_compat_prepare_path(char const *path,
                                  char **buffer_out,
                                  char const **libc_path_out);
int img2sixel_compat_chmod(const char *path, mode_t mode);
#if !defined(HAVE_MKSTEMP)
int img2sixel_compat_mktemp(char *templ, size_t buffer_size);
int img2sixel_compat_open(const char *path, int flags, ...);
#endif
int img2sixel_compat_close(int fd);
int img2sixel_compat_unlink(const char *path);
int img2sixel_compat_access(const char *path, int mode);
int img2sixel_compat_stat(const char *path, struct stat *stat_buffer);
int img2sixel_compat_rename(const char *src_path, const char *dst_path);
ssize_t img2sixel_compat_write(int fd, const void *buffer, size_t count);
void img2sixel_compat_puts(const char *buf);

#endif  /* IMG2SIXEL_COMPAT_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
