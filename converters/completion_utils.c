/*
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

#include "config.h"
#include "completion_utils.h"

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

#include "../src/compat_stub.h"

/* _WIN32 */
#if HAVE_DIRECT_H
# include <direct.h>
#endif  /* HAVE_DIRECT_H */
#if HAVE_IO_H
# include <io.h>
#endif  /* HAVE_IO_H */

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

#if defined(HAVE_COMPLETION_EMBED_H)
#include "completion_embed.h"
#define IMG2SIXEL_HAVE_COMPLETION_EMBED 1
#elif defined(__has_include)
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
/* helpers for platform abstractions */
/* ------------------------------------------------------------------------ */

static int
img2sixel_fsync(int fd)
{
#if HAVE__COMMIT
    return _commit(fd);
#elif HAVE_FSYNC
    return fsync(fd);
#else
    return (0);
#endif
}

#if HAVE__MKDIR
int _mkdir (const char *);
#endif

static int
img2sixel_mkdir(const char *path, mode_t mode)
{
#if HAVE__MKDIR
    (void)mode;
    return _mkdir(path);
#elif HAVE_MKDIR
    return mkdir(path, mode);
#else
    return (-1);
#endif
}

static void
img2sixel_log_errno(const char *fmt, ...)
{
    va_list ap;
    char errbuf[128];

    va_start(ap, fmt);
#if HAVE_DIAGNOSTIC_FORMAT_NONLITERAL
# if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wformat-nonliteral"
# elif defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wformat-nonliteral"
# endif
#endif
    vfprintf(stderr, fmt, ap);
#if HAVE_DIAGNOSTIC_FORMAT_NONLITERAL
# if defined(__clang__)
#  pragma clang diagnostic pop
# elif defined(__GNUC__)
#  pragma GCC diagnostic pop
# endif
#endif
    va_end(ap);
    if (errno != 0) {
        if (sixel_compat_strerror(errno, errbuf, sizeof(errbuf)) != NULL) {
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

    if (stat(path, &st) != 0) {
        return -1;
    }

    if (st.st_size < 0) {
        errno = EINVAL;
        return -1;
    }

    fp = sixel_compat_fopen(path, "rb");
    if (fp == NULL) {
        return -1;
    }

    tmp = (char *)malloc((size_t)st.st_size + 1);
    if (tmp == NULL) {
        fclose(fp);
        return -1;
    }

    read_len = fread(tmp, 1, (size_t)st.st_size, fp);
    if (read_len != (size_t)st.st_size) {
        free(tmp);
        fclose(fp);
        errno = EIO;
        return -1;
    }

    if (fclose(fp) != 0) {
        free(tmp);
        return -1;
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
    if (sixel_compat_mktemp(tmp_path, dst_len + suffix_len + 1) != 0) {
        free(tmp_path);
        return -1;
    }
    fd = sixel_compat_open(tmp_path,
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
        int saved_errno;

        saved_errno = errno;
        (void)sixel_compat_close(fd);
        (void)sixel_compat_unlink(tmp_path);
        free(tmp_path);
        errno = saved_errno;
        return -1;
    }
#else
    (void)mode;
#endif

    total = 0;
    while (total < len) {
        written = sixel_compat_write(fd,
                                     (const char *)buf + total,
                                     len - total);
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            saved_errno = errno;
            (void)sixel_compat_close(fd);
            (void)sixel_compat_unlink(tmp_path);
            free(tmp_path);
            errno = saved_errno;
            return -1;
        }
        total += (size_t)written;
    }

    if (img2sixel_fsync(fd) != 0) {
        int saved_errno;

        saved_errno = errno;
        (void)sixel_compat_close(fd);
        (void)sixel_compat_unlink(tmp_path);
        free(tmp_path);
        errno = saved_errno;
        return -1;
    }

    if (sixel_compat_close(fd) != 0) {
        saved_errno = errno;
        (void)sixel_compat_unlink(tmp_path);
        free(tmp_path);
        errno = saved_errno;
        return -1;
    }

    if (rename(tmp_path, dst_path) != 0) {
        saved_errno = errno;
        (void)sixel_compat_unlink(tmp_path);
        free(tmp_path);
        errno = saved_errno;
        return -1;
    }

    free(tmp_path);
    return 0;
}

int
ensure_dir_p(const char *path, mode_t mode)
{
    size_t len;
    char *tmp;
    size_t i;

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

    for (i = 1; i < len; ++i) {
        if (tmp[i] == '/') {
            char saved;

            saved = tmp[i];
            tmp[i] = '\0';
            if (tmp[0] != '\0') {
#if defined(_WIN32)
                /*
                 * Drive-qualified paths include a `letter:` prefix.  The
                 * ladder below sketches how we peel the segments without
                 * attempting to `mkdir("d:")`:
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
#endif
                if (img2sixel_mkdir(tmp, mode) != 0 && errno != EEXIST) {
                    int saved_errno;

                    saved_errno = errno;
                    free(tmp);
                    errno = saved_errno;
                    return -1;
                }
            }
            tmp[i] = saved;
        }
    }

    if (img2sixel_mkdir(tmp, mode) != 0 && errno != EEXIST) {
        int saved_errno;

        saved_errno = errno;
        free(tmp);
        errno = saved_errno;
        return -1;
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

    fp = sixel_compat_fopen(path, "rb");
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

    fp = sixel_compat_fopen(path, "ab");
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
img2sixel_read_if_exists(const char *path, char **out, size_t *len)
{
    if (path == NULL) {
        return -1;
    }
    if (sixel_compat_access(path, R_OK) != 0) {
        return -1;
    }
    return read_entire_file(path, out, len);
}

static int
img2sixel_try_env(const char *env_name, char **out, size_t *len)
{
    const char *path;

    path = sixel_compat_getenv(env_name);
    if (path == NULL || path[0] == '\0') {
        return -1;
    }

    return img2sixel_read_if_exists(path, out, len);
}

static int
img2sixel_try_env_dir(const char *env_name, const char *suffix,
                      char **out, size_t *len)
{
    const char *dir;
    char *candidate;
    int ret;

    dir = sixel_compat_getenv(env_name);
    if (dir == NULL || dir[0] == '\0') {
        return -1;
    }

    if (img2sixel_join_path(dir, suffix, &candidate) != 0) {
        return -1;
    }

    ret = img2sixel_read_if_exists(candidate, out, len);
    free(candidate);

    return ret;
}

static int
img2sixel_try_pkgdatadir(const char *suffix, char **out, size_t *len)
{
#if defined(PKGDATADIR)
    char *candidate;
    int ret;

    if (img2sixel_join_path(PKGDATADIR, suffix, &candidate) != 0) {
        return -1;
    }

    ret = img2sixel_read_if_exists(candidate, out, len);
    free(candidate);
    return ret;
#else
    (void)suffix;
    (void)out;
    (void)len;
    return -1;
#endif
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
get_completion_text(const char *shell, const char *from_override,
                    char **out, size_t *len)
{
    const char *env_single;
    const char *env_dir_suffix;
    int ret;

    if (shell == NULL || out == NULL || len == NULL) {
        errno = EINVAL;
        return -1;
    }

    *out = NULL;
    *len = 0;

    if (from_override != NULL && from_override[0] != '\0') {
        if (img2sixel_read_if_exists(from_override, out, len) == 0) {
            return 0;
        }
    }

    env_single = NULL;
    env_dir_suffix = NULL;

    if (strcmp(shell, "bash") == 0) {
        env_single = "IMG2SIXEL_COMPLETION_BASH";
        env_dir_suffix = "/bash/img2sixel";
    } else if (strcmp(shell, "zsh") == 0) {
        env_single = "IMG2SIXEL_COMPLETION_ZSH";
        env_dir_suffix = "/zsh/_img2sixel";
    } else {
        errno = EINVAL;
        return -1;
    }

    if (env_single != NULL) {
        if (img2sixel_try_env(env_single, out, len) == 0) {
            return 0;
        }
    }

    if (env_dir_suffix != NULL) {
        if (img2sixel_try_env_dir("IMG2SIXEL_COMPLETION_DIR",
                                  env_dir_suffix, out, len) == 0) {
            return 0;
        }
    }

    if (strcmp(shell, "bash") == 0) {
        ret = img2sixel_try_pkgdatadir(
            "/converters/shell-completion/bash/img2sixel", out, len);
        if (ret == 0) {
            return 0;
        }
        ret = img2sixel_try_pkgdatadir(
            "/bash-completion/completions/img2sixel", out, len);
        if (ret == 0) {
            return 0;
        }
    } else {
        ret = img2sixel_try_pkgdatadir(
            "/converters/shell-completion/zsh/_img2sixel", out, len);
        if (ret == 0) {
            return 0;
        }
        ret = img2sixel_try_pkgdatadir(
            "/zsh/site-functions/_img2sixel", out, len);
        if (ret == 0) {
            return 0;
        }
    }

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

    home = sixel_compat_getenv("IMG2SIXEL_COMPLETION_HOME");
    if (home != NULL && home[0] != '\0') {
        return home;
    }

    home = sixel_compat_getenv("HOME");
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
    version = sixel_compat_getenv("BASH_VERSION");
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
        && sixel_compat_access(fallback_path, F_OK) == 0) {
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
        if (get_completion_text("bash", NULL, &buf, &len) != 0) {
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
        if (get_completion_text("zsh", NULL, &buf, &len) != 0) {
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
        if (get_completion_text("bash", NULL, &buf, &len) != 0) {
            img2sixel_log_errno("failed to load bash completion data");
            return -1;
        }
        printf("# ---- bash ----\n");
        fwrite(buf, 1, len, stdout);
        if (len == 0 || buf[len - 1] != '\n') {
            printf("\n");
        }
        free(buf);

        if (get_completion_text("zsh", NULL, &buf, &len) != 0) {
            img2sixel_log_errno("failed to load zsh completion data");
            return -1;
        }
        printf("# ---- zsh ----\n");
        fwrite(buf, 1, len, stdout);
        if (len == 0 || buf[len - 1] != '\n') {
            printf("\n");
        }
        free(buf);

        return 0;
    }

    if ((mask & IMG2SIXEL_COMPLETION_SHELL_BASH) != 0) {
        if (get_completion_text("bash", NULL, &buf, &len) != 0) {
            img2sixel_log_errno("failed to load bash completion data");
            return -1;
        }
        fwrite(buf, 1, len, stdout);
        if (len == 0 || buf[len - 1] != '\n') {
            printf("\n");
        }
        free(buf);
    }

    if ((mask & IMG2SIXEL_COMPLETION_SHELL_ZSH) != 0) {
        if (get_completion_text("zsh", NULL, &buf, &len) != 0) {
            img2sixel_log_errno("failed to load zsh completion data");
            return -1;
        }
        fwrite(buf, 1, len, stdout);
        if (len == 0 || buf[len - 1] != '\n') {
            printf("\n");
        }
        free(buf);
    }

    return 0;
}

static int
img2sixel_unlink_result(const char *path)
{
    if (sixel_compat_unlink(path) == 0) {
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

    if (strcmp(action, "show") == 0) {
        if (img2sixel_handle_show(mask) != 0) {
            *exit_code = EXIT_FAILURE;
            return -1;
        }
        *exit_code = EXIT_SUCCESS;
        return 1;
    }

    if (strcmp(action, "install") == 0) {
        if (img2sixel_handle_install(mask) != 0) {
            *exit_code = EXIT_FAILURE;
            return -1;
        }
        *exit_code = EXIT_SUCCESS;
        return 1;
    }

    if (strcmp(action, "uninstall") == 0) {
        if (img2sixel_handle_uninstall(mask) != 0) {
            *exit_code = EXIT_FAILURE;
            return -1;
        }
        *exit_code = EXIT_SUCCESS;
        return 1;
    }

    fprintf(stderr, "unexpected completion action: %s\n", action);
    *exit_code = EXIT_FAILURE;
    return -1;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
