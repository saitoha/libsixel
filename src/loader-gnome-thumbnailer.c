/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2021-2025 libsixel developers. See `AUTHORS`.
 * Copyright (c) 2014-2019 Hayaki Saito
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
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF, OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * GNOME/desktop thumbnailer orchestration split from loader.c so the heavy
 * process management and file(1) helpers stay behind platform guards.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#if !defined(_POSIX_C_SOURCE)
# define _POSIX_C_SOURCE 200809L
#endif

/* STDC_HEADERS */
#include <stdio.h>
#include <stdlib.h>

#if HAVE_STRING_H
# include <string.h>
#endif
#if HAVE_CTYPE_H
# include <ctype.h>
#endif
#if HAVE_STDARG_H
# include <stdarg.h>
#endif
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#if !defined(PATH_MAX)
# define PATH_MAX 4096
#endif
#if HAVE_FCNTL_H
# include <fcntl.h>
#endif
#if HAVE_SYS_TIME_H
# include <sys/time.h>
#elif HAVE_TIME_H
# include <time.h>
#endif  /* HAVE_SYS_TIME_H HAVE_TIME_H */
#if HAVE_ERRNO_H
# include <errno.h>
#endif
#if HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif
#if HAVE_SPAWN_H
# include <spawn.h>
#endif
#if HAVE_LIMITS_H
# include <limits.h>
#endif
#if HAVE_DIRENT_H
# include <dirent.h>
#endif

#if HAVE_SPAWN_H && HAVE_POSIX_SPAWNP && !defined(__FreeBSD__) && !defined(__DragonFly__)
/*
 * FreeBSD/DragonflyBSD's libc does not export `environ` from shared libraries when they
 * are linked with --no-undefined.  Restrict the declaration to platforms
 * that will actually route thumbnailer_spawn() through posix_spawnp().
 */
extern char **environ;
#endif

#include <sixel.h>

#include "chunk.h"
#include "compat_stub.h"
#include "loader-builtin.h"
#include "loader-common.h"
#include "loader-gnome-thumbnailer.h"
#include "loader.h"
#include "logger.h"
#include "sleep.h"

#if HAVE_FREEDESKTOP_THUMBNAILING

# if defined(HAVE_MKSTEMP)
int mkstemp(char *);
# endif

enum { THUMBNAILER_POLL_SLEEP_USEC = 10000u }; /* 10 ms */

/*
 * thumbnailer_message_finalize
 *
 * Clamp formatted messages so callers do not have to repeat truncation
 * checks after calling sixel_compat_snprintf().
 */
static void
thumbnailer_message_finalize(char *buffer, size_t capacity, int written)
{
    if (buffer == NULL || capacity == 0) {
        return;
    }

    if (written < 0) {
        buffer[0] = '\0';
        return;
    }

    if ((size_t)written >= capacity) {
        buffer[capacity - 1u] = '\0';
    }
}

/*
 * thumbnailer_resolve_path
 *
 * Resolve the supplied path to an absolute canonical path when possible.
 *
 * Arguments:
 *     path - original filesystem path.
 * Returns:
 *     Newly allocated canonical path or NULL on failure.
 */
static char *
thumbnailer_resolve_path(char const *path)
{
    char *resolved;

    resolved = NULL;

    if (path == NULL) {
        return NULL;
    }

    /*
     * Delegate platform quirks to the shared compatibility layer so
     * NetBSD/MinGW builds avoid implicit CRT declarations.
     */
    resolved = sixel_compat_realpath(path);

    return resolved;
}

struct thumbnailer_string_list {
    char **items;
    size_t length;
    size_t capacity;
};

struct thumbnailer_entry {
    char *exec_line;
    char *tryexec;
    struct thumbnailer_string_list *mime_types;
};

static int thumbnailer_parse_file(char const *path,
                                  struct thumbnailer_entry *entry);
static int thumbnailer_has_tryexec(char const *tryexec);
static int thumbnailer_supports_mime(struct thumbnailer_entry *entry,
                                     char const *mime_type);
int thumbnailer_has_fallback_thumbnailer(void);

/*
 * thumbnailer_strdup
 *
 * Duplicate a string with malloc so thumbnail helpers own their copies.
 *
 * Arguments:
 *     src - zero-terminated string to copy; may be NULL.
 * Returns:
 *     Newly allocated duplicate or NULL on failure/NULL input.
 */
char *
thumbnailer_strdup(char const *src)
{
    char *copy;
    size_t length;

    copy = NULL;
    length = 0;

    if (src == NULL) {
        return NULL;
    }

    length = strlen(src);
    copy = malloc(length + 1);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, src, length + 1);

    return copy;
}

/*
 * thumbnailer_string_list_new
 *
 * Allocate an empty expandable string list used throughout the loader.
 *
 * Arguments:
 *     None.
 * Returns:
 *     Newly allocated list instance or NULL on failure.
 */
static struct thumbnailer_string_list *
thumbnailer_string_list_new(void)
{
    struct thumbnailer_string_list *list;

    list = malloc(sizeof(*list));
    if (list == NULL) {
        return NULL;
    }

    list->items = NULL;
    list->length = 0;
    list->capacity = 0;

    return list;
}

/*
 * thumbnailer_string_list_free
 *
 * Release every string stored in the list and free the container itself.
 *
 * Arguments:
 *     list - list instance produced by thumbnailer_string_list_new().
 */
static void
thumbnailer_string_list_free(struct thumbnailer_string_list *list)
{
    size_t index;

    index = 0;

    if (list == NULL) {
        return;
    }

    if (list->items != NULL) {
        for (index = 0; index < list->length; ++index) {
            free(list->items[index]);
            list->items[index] = NULL;
        }
        free(list->items);
        list->items = NULL;
    }

    free(list);
}

/*
 * thumbnailer_string_list_append
 *
 * Append a copy of the supplied string to the dynamic list.
 *
 * Arguments:
 *     list  - destination list.
 *     value - string to duplicate and append.
 * Returns:
 *     1 on success, 0 on allocation failure or invalid input.
 */
static int
thumbnailer_string_list_append(struct thumbnailer_string_list *list,
                               char const *value)
{
    size_t new_capacity;
    char **new_items;
    char *copy;

    new_capacity = 0;
    new_items = NULL;
    copy = NULL;

    if (list == NULL || value == NULL) {
        return 0;
    }

    copy = thumbnailer_strdup(value);
    if (copy == NULL) {
        return 0;
    }

    if (list->length == list->capacity) {
        new_capacity = (list->capacity == 0) ? 4 : list->capacity * 2;
        new_items = realloc(list->items,
                            new_capacity * sizeof(*list->items));
        if (new_items == NULL) {
            free(copy);
            return 0;
        }
        list->items = new_items;
        list->capacity = new_capacity;
    }

    list->items[list->length] = copy;
    list->length += 1;

    return 1;
}

/*
 * thumbnailer_entry_init
 *
 * Prepare a thumbnailer_entry structure for population.
 *
 * Arguments:
 *     entry - caller-provided structure to initialize.
 */
static void
thumbnailer_entry_init(struct thumbnailer_entry *entry)
{
    if (entry == NULL) {
        return;
    }

    entry->exec_line = NULL;
    entry->tryexec = NULL;
    entry->mime_types = NULL;
}

/*
 * thumbnailer_entry_clear
 *
 * Release every heap allocation associated with a thumbnailer_entry.
 *
 * Arguments:
 *     entry - structure previously initialized with thumbnailer_entry_init().
 */
static void
thumbnailer_entry_clear(struct thumbnailer_entry *entry)
{
    if (entry == NULL) {
        return;
    }

    free(entry->exec_line);
    entry->exec_line = NULL;
    free(entry->tryexec);
    entry->tryexec = NULL;
    thumbnailer_string_list_free(entry->mime_types);
    entry->mime_types = NULL;
}

/*
 * thumbnailer_join_paths
 *
 * Concatenate two path fragments inserting a slash when required.
 *
 * Arguments:
 *     left  - directory prefix.
 *     right - trailing component.
 * Returns:
 *     Newly allocated combined path or NULL on failure.
 */
static char *
thumbnailer_join_paths(char const *left, char const *right)
{
    size_t left_length;
    size_t right_length;
    int need_separator;
    char *combined;

    left_length = 0;
    right_length = 0;
    need_separator = 0;
    combined = NULL;

    if (left == NULL || right == NULL) {
        return NULL;
    }

    left_length = strlen(left);
    right_length = strlen(right);
    need_separator = 0;

    if (left_length > 0 && right_length > 0 &&
            left[left_length - 1] != '/' && right[0] != '/') {
        need_separator = 1;
    }

    combined = malloc(left_length + right_length + need_separator + 1);
    if (combined == NULL) {
        return NULL;
    }

    memcpy(combined, left, left_length);
    if (need_separator) {
        combined[left_length] = '/';
        memcpy(combined + left_length + 1, right, right_length);
        combined[left_length + right_length + 1] = '\0';
    } else {
        memcpy(combined + left_length, right, right_length);
        combined[left_length + right_length] = '\0';
    }

    return combined;
}

/*
 * thumbnailer_collect_directories
 *
 * Enumerate directories that may contain FreeDesktop thumbnailer
 * definitions according to the XDG specification.
 *
 * GNOME thumbnailers follow the XDG data directory contract:
 *
 *     +------------------+      +---------------------------+
 *     | HOME/.local/share| ---> | HOME/.local/share/        |
 *     |                  |      |    thumbnailers/(*.thumbnailer)
 *     +------------------+      +---------------------------+
 *
 *     +------------------+      +---------------------------+
 *     | XDG_DATA_DIRS    | ---> | <dir>/thumbnailers/(*.thumbnailer)
 *     +------------------+      +---------------------------+
 *
 * The helper below expands both sources so that the caller can iterate
 * through every known definition in order of precedence.
 *
 * Arguments:
 *     None.
 * Returns:
 *     Newly allocated list of directory paths or NULL on failure.
 */
static struct thumbnailer_string_list *
thumbnailer_collect_directories(void)
{
    struct thumbnailer_string_list *dirs;
    char const *xdg_data_dirs;
    char const *home_dir;
    char const *default_dirs;
    char *candidate;
    char *local_share;
    char *dirs_copy;
    char *token;
    char *token_context;

    dirs = NULL;
    xdg_data_dirs = NULL;
    home_dir = NULL;
    default_dirs = NULL;
    candidate = NULL;
    local_share = NULL;
    dirs_copy = NULL;
    token = NULL;
    token_context = NULL;

    dirs = thumbnailer_string_list_new();
    if (dirs == NULL) {
        return NULL;
    }

    home_dir = sixel_compat_getenv("HOME");
    loader_trace_message(
        "thumbnailer_collect_directories: HOME=%s",
        (home_dir != NULL && home_dir[0] != '\0') ? home_dir : "(unset)");
    if (home_dir != NULL && home_dir[0] != '\0') {
        local_share = thumbnailer_join_paths(home_dir,
                                             ".local/share");
        if (local_share != NULL) {
            candidate = thumbnailer_join_paths(local_share,
                                               "thumbnailers");
            if (candidate != NULL) {
                if (!thumbnailer_string_list_append(dirs, candidate)) {
                    free(candidate);
                    free(local_share);
                    thumbnailer_string_list_free(dirs);
                    return NULL;
                }
                loader_trace_message(
                    "thumbnailer_collect_directories: added %s",
                    candidate);
                free(candidate);
                candidate = NULL;
            }
            free(local_share);
            local_share = NULL;
        }
    }

    xdg_data_dirs = sixel_compat_getenv("XDG_DATA_DIRS");
    if (xdg_data_dirs == NULL || xdg_data_dirs[0] == '\0') {
        default_dirs = "/usr/local/share:/usr/share";
        xdg_data_dirs = default_dirs;
    }
    loader_trace_message(
        "thumbnailer_collect_directories: XDG_DATA_DIRS=%s",
        xdg_data_dirs);

    dirs_copy = thumbnailer_strdup(xdg_data_dirs);
    if (dirs_copy == NULL) {
        thumbnailer_string_list_free(dirs);
        return NULL;
    }
    token = sixel_compat_strtok(dirs_copy, ":", &token_context);
    while (token != NULL) {
        candidate = thumbnailer_join_paths(token, "thumbnailers");
        if (candidate != NULL) {
            if (!thumbnailer_string_list_append(dirs, candidate)) {
                free(candidate);
                free(dirs_copy);
                thumbnailer_string_list_free(dirs);
                return NULL;
            }
            loader_trace_message(
                "thumbnailer_collect_directories: added %s",
                candidate);
            free(candidate);
            candidate = NULL;
        }
        token = sixel_compat_strtok(NULL, ":", &token_context);
    }
    free(dirs_copy);
    dirs_copy = NULL;

    return dirs;
}

void
loader_probe_gnome_thumbnailers(char const *mime_type,
                                int *has_directories,
                                int *has_match)
{
    struct thumbnailer_string_list *directories;
    struct thumbnailer_entry info;
    size_t dir_index;
    DIR *dir;
    struct dirent *entry;
    char *thumbnailer_path;
    int match;
    int directories_present;
    size_t name_length;

    directories = NULL;
    dir_index = 0;
    dir = NULL;
    entry = NULL;
    thumbnailer_path = NULL;
    match = 0;
    directories_present = 0;
    name_length = 0;

    if (has_directories != NULL) {
        *has_directories = 0;
    }
    if (has_match != NULL) {
        *has_match = 0;
    }

    directories = thumbnailer_collect_directories();
    if (directories == NULL) {
        return;
    }

    if (directories->length > 0) {
        directories_present = 1;
        if (has_directories != NULL) {
            *has_directories = 1;
        }
    }

    thumbnailer_entry_init(&info);

    if (mime_type != NULL && mime_type[0] != '\0') {
        for (dir_index = 0; dir_index < directories->length && match == 0;
                ++dir_index) {
            dir = opendir(directories->items[dir_index]);
            if (dir == NULL) {
                continue;
            }
            while (match == 0 && (entry = readdir(dir)) != NULL) {
                thumbnailer_entry_clear(&info);
                thumbnailer_entry_init(&info);
                name_length = strlen(entry->d_name);
                if (name_length < 12 ||
                        strcmp(entry->d_name + name_length - 12,
                               ".thumbnailer") != 0) {
                    continue;
                }
                thumbnailer_path = thumbnailer_join_paths(
                    directories->items[dir_index],
                    entry->d_name);
                if (thumbnailer_path == NULL) {
                    continue;
                }
                if (!thumbnailer_parse_file(thumbnailer_path, &info)) {
                    free(thumbnailer_path);
                    thumbnailer_path = NULL;
                    continue;
                }
                free(thumbnailer_path);
                thumbnailer_path = NULL;
                if (!thumbnailer_has_tryexec(info.tryexec)) {
                    continue;
                }
                if (thumbnailer_supports_mime(&info, mime_type)) {
                    match = 1;
                }
            }
            closedir(dir);
            dir = NULL;
        }
    }

    thumbnailer_entry_clear(&info);
    thumbnailer_string_list_free(directories);

    if (directories_present && has_match != NULL) {
        *has_match = match;
    }
}

/*
 * thumbnailer_trim_right
 *
 * Remove trailing whitespace in place from a mutable string.
 *
 * Arguments:
 *     text - string to trim; must be writable and zero-terminated.
 */
static void
thumbnailer_trim_right(char *text)
{
    size_t length;

    length = 0;

    if (text == NULL) {
        return;
    }

    length = strlen(text);
    while (length > 0 && isspace((unsigned char)text[length - 1]) != 0) {
        text[length - 1] = '\0';
        length -= 1;
    }
}

/*
 * thumbnailer_trim_left
 *
 * Skip leading whitespace so parsers can focus on significant tokens.
 *
 * Arguments:
 *     text - string to inspect; may be NULL.
 * Returns:
 *     Pointer to first non-space character or NULL when input is NULL.
 */
static char *
thumbnailer_trim_left(char *text)
{
    if (text == NULL) {
        return NULL;
    }

    while (*text != '\0' && isspace((unsigned char)*text) != 0) {
        text += 1;
    }

    return text;
}

/*
 * thumbnailer_parse_file
 *
 * Populate a thumbnailer_entry by parsing a .thumbnailer ini file.
 *
 * Arguments:
 *     path  - filesystem path to the ini file.
 *     entry - output structure initialized with thumbnailer_entry_init().
 * Returns:
 *     1 on success, 0 on parse error or allocation failure.
 */
static int
thumbnailer_parse_file(char const *path, struct thumbnailer_entry *entry)
{
    FILE *fp;
    char line[1024];
    int in_group;
    char *trimmed;
    char *key_end;
    char *value;
    char *token_start;
    char *token_end;
    struct thumbnailer_string_list *mime_types;
    size_t index;

    fp = NULL;
    in_group = 0;
    trimmed = NULL;
    key_end = NULL;
    value = NULL;
    token_start = NULL;
    token_end = NULL;
    mime_types = NULL;
    index = 0;

    if (path == NULL || entry == NULL) {
        return 0;
    }

    fp = sixel_compat_fopen(path, "r");
    if (fp == NULL) {
        return 0;
    }

    mime_types = thumbnailer_string_list_new();
    if (mime_types == NULL) {
        fclose(fp);
        fp = NULL;
        return 0;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        trimmed = thumbnailer_trim_left(line);
        thumbnailer_trim_right(trimmed);
        if (trimmed[0] == '\0' || trimmed[0] == '#' || trimmed[0] == ';') {
            continue;
        }
        if (trimmed[0] == '[') {
            key_end = strchr(trimmed, ']');
            if (key_end != NULL) {
                *key_end = '\0';
                if (strcmp(trimmed + 1, "Thumbnailer Entry") == 0) {
                    in_group = 1;
                } else {
                    in_group = 0;
                }
            }
            continue;
        }
        if (!in_group) {
            continue;
        }
        key_end = strchr(trimmed, '=');
        if (key_end == NULL) {
            continue;
        }
        *key_end = '\0';
        value = thumbnailer_trim_left(key_end + 1);
        thumbnailer_trim_right(trimmed);
        thumbnailer_trim_right(value);
        if (strcmp(trimmed, "Exec") == 0) {
            free(entry->exec_line);
            entry->exec_line = thumbnailer_strdup(value);
            if (entry->exec_line == NULL) {
                fclose(fp);
                fp = NULL;
                thumbnailer_string_list_free(mime_types);
                mime_types = NULL;
                return 0;
            }
        } else if (strcmp(trimmed, "TryExec") == 0) {
            free(entry->tryexec);
            entry->tryexec = thumbnailer_strdup(value);
            if (entry->tryexec == NULL) {
                fclose(fp);
                fp = NULL;
                thumbnailer_string_list_free(mime_types);
                mime_types = NULL;
                return 0;
            }
        } else if (strcmp(trimmed, "MimeType") == 0) {
            for (index = 0; index < mime_types->length; ++index) {
                free(mime_types->items[index]);
                mime_types->items[index] = NULL;
            }
            mime_types->length = 0;
            token_start = value;
            while (token_start != NULL && token_start[0] != '\0') {
                token_end = strchr(token_start, ';');
                if (token_end != NULL) {
                    *token_end = '\0';
                }
                token_start = thumbnailer_trim_left(token_start);
                thumbnailer_trim_right(token_start);
                if (token_start[0] != '\0') {
                    if (!thumbnailer_string_list_append(mime_types,
                                                       token_start)) {
                        fclose(fp);
                        fp = NULL;
                        thumbnailer_string_list_free(mime_types);
                        mime_types = NULL;
                        return 0;
                    }
                }
                if (token_end == NULL) {
                    break;
                }
                token_start = token_end + 1;
            }
        }
    }

    fclose(fp);
    fp = NULL;

    thumbnailer_string_list_free(entry->mime_types);
    entry->mime_types = mime_types;

    return 1;
}

/*
 * thumbnailer_has_tryexec
 *
 * Confirm that the optional TryExec binary exists and is executable.
 *
 * Arguments:
 *     tryexec - value from the .thumbnailer file; may be NULL.
 * Returns:
 *     1 when executable, 0 otherwise.
 */
static int
thumbnailer_has_tryexec(char const *tryexec)
{
    char const *path_variable;
    char const *start;
    char const *end;
    size_t length;
    char *candidate;
    int executable;

    path_variable = NULL;
    start = NULL;
    end = NULL;
    length = 0;
    candidate = NULL;
    executable = 0;

    if (tryexec == NULL || tryexec[0] == '\0') {
        return 1;
    }

    if (strchr(tryexec, '/') != NULL) {
        if (sixel_compat_access(tryexec, X_OK) == 0) {
            return 1;
        }
        return 0;
    }

    path_variable = sixel_compat_getenv("PATH");
    if (path_variable == NULL) {
        return 0;
    }

    start = path_variable;
    while (*start != '\0') {
        end = strchr(start, ':');
        if (end == NULL) {
            end = start + strlen(start);
        }
        length = (size_t)(end - start);
        candidate = malloc(length + strlen(tryexec) + 2);
        if (candidate == NULL) {
            return 0;
        }
        memcpy(candidate, start, length);
        candidate[length] = '/';
        sixel_compat_strcpy(candidate + length + 1,
                            strlen(tryexec) + 1u,
                            tryexec);
        if (sixel_compat_access(candidate, X_OK) == 0) {
            executable = 1;
            free(candidate);
            candidate = NULL;
            break;
        }
        free(candidate);
        candidate = NULL;
        if (*end == '\0') {
            break;
        }
        start = end + 1;
    }

    return executable;
}

/*
 * thumbnailer_has_fallback_thumbnailer
 *
 * Confirm that the built-in gdk-pixbuf fallback is runnable.  Tests use
 * this to skip when the thumbnailer binary is absent from PATH or fails
 * to start because of missing runtime dependencies.
 *
 * Arguments:
 *     None.
 * Returns:
 *     1 when executable, 0 otherwise.
 */
int
thumbnailer_has_fallback_thumbnailer(void)
{
    pid_t pid;
    int status;
    int wait_result;
    int executable;
    int devnull;
    int dup_status;

    pid = (-1);
    status = 0;
    wait_result = 0;
    executable = thumbnailer_has_tryexec("gdk-pixbuf-thumbnailer");
    devnull = -1;
    dup_status = 0;

    if (executable == 0) {
        return 0;
    }

    pid = fork();
    if (pid < 0) {
        return 0;
    }
    if (pid == 0) {
        /* Silence usage text so test harness output remains clean. */
        devnull = sixel_compat_open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup_status = dup2(devnull, STDOUT_FILENO);
            if (dup_status >= 0) {
                dup_status = dup2(devnull, STDERR_FILENO);
            }
            sixel_compat_close(devnull);
            devnull = -1;
        }
        if (dup_status < 0) {
            _exit(127);
        }
        execlp("gdk-pixbuf-thumbnailer",
               "gdk-pixbuf-thumbnailer",
               "--help",
               NULL);
        _exit(127);
    }

    while ((wait_result = waitpid(pid, &status, 0)) < 0 &&
            errno == EINTR) {
        continue;
    }

    if (wait_result < 0) {
        return 0;
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        return 0;
    }

    return 1;
}

/*
 * thumbnailer_mime_matches
 *
 * Test whether a thumbnailer MIME pattern matches the probed MIME type.
 *
 * Arguments:
 *     pattern   - literal MIME pattern or prefix ending with "slash-asterisk".
 *     mime_type - MIME value obtained from file --mime-type.
 * Returns:
 *     1 when the pattern applies, 0 otherwise.
 */
static int
thumbnailer_mime_matches(char const *pattern, char const *mime_type)
{
    size_t length;

    length = 0;

    if (pattern == NULL || mime_type == NULL) {
        return 0;
    }

    if (strcmp(pattern, mime_type) == 0) {
        return 1;
    }

    length = strlen(pattern);
    if (length >= 2 && pattern[length - 1] == '*' &&
            pattern[length - 2] == '/') {
        return strncmp(pattern, mime_type, length - 1) == 0;
    }

    return 0;
}

/*
 * thumbnailer_supports_mime
 *
 * Iterate over MIME patterns advertised by a thumbnailer entry.
 *
 * Arguments:
 *     entry     - parsed thumbnailer entry with mime_types list.
 *     mime_type - MIME type string to match.
 * Returns:
 *     1 when a match is found, 0 otherwise.
 */
static int
thumbnailer_supports_mime(struct thumbnailer_entry *entry,
                          char const *mime_type)
{
    size_t index;

    index = 0;

    if (entry == NULL || entry->mime_types == NULL) {
        return 0;
    }

    if (mime_type == NULL) {
        return 0;
    }

    for (index = 0; index < entry->mime_types->length; ++index) {
        if (thumbnailer_mime_matches(entry->mime_types->items[index],
                                     mime_type)) {
            return 1;
        }
    }

    return 0;
}

/*
 * thumbnailer_shell_quote
 *
 * Produce a single-quoted variant of an argument for readable logging.
 *
 * Arguments:
 *     text - unquoted argument.
 * Returns:
 *     Newly allocated quoted string or NULL on allocation failure.
 */
static char *
thumbnailer_shell_quote(char const *text)
{
    size_t index;
    size_t length;
    size_t needed;
    char *quoted;
    size_t position;

    index = 0;
    length = 0;
    needed = 0;
    quoted = NULL;
    position = 0;

    if (text == NULL) {
        return NULL;
    }

    length = strlen(text);
    needed = 2;
    for (index = 0; index < length; ++index) {
        if (text[index] == '\'') {
            needed += 4;
        } else {
            needed += 1;
        }
    }

    quoted = malloc(needed + 1);
    if (quoted == NULL) {
        return NULL;
    }

    quoted[position++] = '\'';
    for (index = 0; index < length; ++index) {
        if (text[index] == '\'') {
            quoted[position++] = '\'';
            quoted[position++] = '\\';
            quoted[position++] = '\'';
            quoted[position++] = '\'';
        } else {
            quoted[position++] = text[index];
        }
    }
    quoted[position++] = '\'';
    quoted[position] = '\0';

    return quoted;
}

struct thumbnailer_builder {
    char *buffer;
    size_t length;
    size_t capacity;
};

/*
 * thumbnailer_builder_reserve
 *
 * Grow the builder buffer so future appends fit without overflow.
 *
 * Arguments:
 *     builder    - mutable builder instance.
 *     additional - number of bytes that must fit excluding terminator.
 * Returns:
 *     1 on success, 0 on allocation failure.
 */
static int
thumbnailer_builder_reserve(struct thumbnailer_builder *builder,
                            size_t additional)
{
    size_t new_capacity;
    char *new_buffer;

    new_capacity = 0;
    new_buffer = NULL;

    if (builder->length + additional + 1 <= builder->capacity) {
        return 1;
    }

    new_capacity = (builder->capacity == 0) ? 64 : builder->capacity;
    while (new_capacity < builder->length + additional + 1) {
        new_capacity *= 2;
    }

    new_buffer = realloc(builder->buffer, new_capacity);
    if (new_buffer == NULL) {
        return 0;
    }

    builder->buffer = new_buffer;
    builder->capacity = new_capacity;

    return 1;
}

/*
 * thumbnailer_builder_append_char
 *
 * Append a single character to the builder.
 *
 * Arguments:
 *     builder - mutable builder instance.
 *     ch      - character to append.
 * Returns:
 *     1 on success, 0 on allocation failure.
 */
static int
thumbnailer_builder_append_char(struct thumbnailer_builder *builder,
                                char ch)
{
    if (!thumbnailer_builder_reserve(builder, 1)) {
        return 0;
    }

    builder->buffer[builder->length] = ch;
    builder->length += 1;
    builder->buffer[builder->length] = '\0';

    return 1;
}

/*
 * thumbnailer_builder_append
 *
 * Append a string of known length to the builder buffer.
 *
 * Arguments:
 *     builder - mutable builder instance.
 *     text    - zero-terminated string to append.
 * Returns:
 *     1 on success, 0 on allocation failure or NULL input.
 */
static int
thumbnailer_builder_append(struct thumbnailer_builder *builder,
                           char const *text)
{
    size_t length;

    length = 0;

    if (text == NULL) {
        return 1;
    }

    length = strlen(text);
    if (!thumbnailer_builder_reserve(builder, length)) {
        return 0;
    }

    memcpy(builder->buffer + builder->length, text, length);
    builder->length += length;
    builder->buffer[builder->length] = '\0';

    return 1;
}

/*
 * thumbnailer_builder_clear
 *
 * Reset builder length to zero while retaining allocated storage.
 *
 * Arguments:
 *     builder - builder to reset.
 */
static void
thumbnailer_builder_clear(struct thumbnailer_builder *builder)
{
    if (builder->buffer != NULL) {
        builder->buffer[0] = '\0';
    }
    builder->length = 0;
}

/*
 * thumbnailer_command owns the argv array that will be passed to the
 * thumbnailer helper.  The display field keeps a human readable command line
 * for verbose logging without recomputing the shell quoted form.
 */
struct thumbnailer_command {
    char **argv;
    size_t argc;
    char *display;
};

/*
 * thumbnailer_command_free
 *
 * Release argv entries, the array itself, and the formatted display copy.
 *
 * Arguments:
 *     command - structure created by thumbnailer_build_command().
 */
static void
thumbnailer_command_free(struct thumbnailer_command *command)
{
    size_t index;

    if (command == NULL) {
        return;
    }

    if (command->argv != NULL) {
        for (index = 0; index < command->argc; ++index) {
            free(command->argv[index]);
            command->argv[index] = NULL;
        }
        free(command->argv);
        command->argv = NULL;
    }

    free(command->display);
    command->display = NULL;

    free(command);
}

/*
 * thumbnailer_command_format
 *
 * Join argv entries into a human-readable command line for logging.
 *
 * Arguments:
 *     argv - array of argument strings.
 *     argc - number of entries stored in argv.
 * Returns:
 *     Newly allocated formatted string or NULL on allocation failure.
 */
static char *
thumbnailer_command_format(char **argv, size_t argc)
{
    struct thumbnailer_builder builder;
    char *quoted;
    size_t index;

    builder.buffer = NULL;
    builder.length = 0;
    builder.capacity = 0;
    quoted = NULL;

    for (index = 0; index < argc; ++index) {
        if (index > 0) {
            if (!thumbnailer_builder_append_char(&builder, ' ')) {
                free(builder.buffer);
                builder.buffer = NULL;
                return NULL;
            }
        }
        quoted = thumbnailer_shell_quote(argv[index]);
        if (quoted == NULL) {
            free(builder.buffer);
            builder.buffer = NULL;
            return NULL;
        }
        if (!thumbnailer_builder_append(&builder, quoted)) {
            free(quoted);
            quoted = NULL;
            free(builder.buffer);
            builder.buffer = NULL;
            return NULL;
        }
        free(quoted);
        quoted = NULL;
    }

    return builder.buffer;
}

/*
 * thumbnailer_build_command
 *
 * Expand a .thumbnailer Exec template into an argv array that honours
 * FreeDesktop substitution rules.
 *
 * Arguments:
 *     template_command - Exec line containing % tokens.
 *     input_path       - filesystem path to the source document.
 *     input_uri        - URI representation for %u expansions.
 *     output_path      - PNG destination path for %o expansions.
 *     size             - numeric size hint passed to %s tokens.
 *     mime_type        - MIME value for %m replacements.
 * Returns:
 *     Newly allocated command or NULL on parse/allocation failure.
 */
static struct thumbnailer_command *
thumbnailer_build_command(char const *template_command,
                          char const *input_path,
                          char const *input_uri,
                          char const *output_path,
                          int size,
                          char const *mime_type)
{
    struct thumbnailer_builder builder;
    struct thumbnailer_string_list *tokens;
    struct thumbnailer_command *command;
    char const *ptr;
    char size_text[16];
    int in_single_quote;
    int in_double_quote;
    int escape_next;
    char const *replacement;
    size_t index;
    int written;

    builder.buffer = NULL;
    builder.length = 0;
    builder.capacity = 0;
    tokens = NULL;
    command = NULL;
    ptr = template_command;
    size_text[0] = '\0';
    in_single_quote = 0;
    in_double_quote = 0;
    escape_next = 0;
    replacement = NULL;
    index = 0;

    if (template_command == NULL) {
        return NULL;
    }

    tokens = thumbnailer_string_list_new();
    if (tokens == NULL) {
        return NULL;
    }

    /*
     * load_with_gnome_thumbnailer() normalizes the hint size to a positive
     * value, so command templates can always substitute a concrete %s token.
     */
    written = sixel_compat_snprintf(size_text,
                                    sizeof(size_text),
                                    "%d",
                                    size);
    if (written < 0) {
        goto error;
    }
    if ((size_t)written >= sizeof(size_text)) {
        size_text[sizeof(size_text) - 1u] = '\0';
    }

    while (ptr != NULL && ptr[0] != '\0') {
        if (!in_single_quote && !in_double_quote && escape_next == 0 &&
                (ptr[0] == ' ' || ptr[0] == '\t')) {
            if (builder.length > 0) {
                if (!thumbnailer_string_list_append(tokens,
                                                    builder.buffer)) {
                    goto error;
                }
                thumbnailer_builder_clear(&builder);
            }
            ptr += 1;
            continue;
        }
        if (!in_single_quote && escape_next == 0 && ptr[0] == '\\') {
            escape_next = 1;
            ptr += 1;
            continue;
        }
        if (!in_double_quote && escape_next == 0 && ptr[0] == '\'') {
            in_single_quote = !in_single_quote;
            ptr += 1;
            continue;
        }
        if (!in_single_quote && escape_next == 0 && ptr[0] == '"') {
            in_double_quote = !in_double_quote;
            ptr += 1;
            continue;
        }
        if (escape_next != 0) {
            if (!thumbnailer_builder_append_char(&builder, ptr[0])) {
                goto error;
            }
            escape_next = 0;
            ptr += 1;
            continue;
        }
        if (ptr[0] == '%' && ptr[1] != '\0') {
            replacement = NULL;
            ptr += 1;
            switch (ptr[0]) {
            case '%':
                if (!thumbnailer_builder_append_char(&builder, '%')) {
                    goto error;
                }
                break;
            case 'i':
            case 'I':
                replacement = input_path;
                break;
            case 'u':
            case 'U':
                replacement = input_uri;
                break;
            case 'o':
            case 'O':
                replacement = output_path;
                break;
            case 's':
            case 'S':
                replacement = size_text;
                break;
            case 'm':
            case 'M':
                replacement = mime_type;
                break;
            default:
                if (!thumbnailer_builder_append_char(&builder, '%') ||
                        !thumbnailer_builder_append_char(&builder,
                                                         ptr[0])) {
                    goto error;
                }
                break;
            }
            if (replacement != NULL) {
                if (!thumbnailer_builder_append(&builder, replacement)) {
                    goto error;
                }
            }
            ptr += 1;
            continue;
        }
        if (!thumbnailer_builder_append_char(&builder, ptr[0])) {
            goto error;
        }
        ptr += 1;
    }

    if (builder.length > 0) {
        if (!thumbnailer_string_list_append(tokens, builder.buffer)) {
            goto error;
        }
    }

    command = malloc(sizeof(*command));
    if (command == NULL) {
        goto error;
    }

    command->argc = tokens->length;
    command->argv = NULL;
    command->display = NULL;

    if (tokens->length == 0) {
        goto error;
    }

    command->argv = malloc(sizeof(char *) * (tokens->length + 1));
    if (command->argv == NULL) {
        goto error;
    }

    for (index = 0; index < tokens->length; ++index) {
        command->argv[index] = thumbnailer_strdup(tokens->items[index]);
        if (command->argv[index] == NULL) {
            goto error;
        }
    }
    command->argv[tokens->length] = NULL;

    command->display = thumbnailer_command_format(command->argv,
                                                  command->argc);
    if (command->display == NULL) {
        goto error;
    }

    thumbnailer_string_list_free(tokens);
    tokens = NULL;
    if (builder.buffer != NULL) {
        free(builder.buffer);
        builder.buffer = NULL;
    }

    return command;

error:
    if (tokens != NULL) {
        thumbnailer_string_list_free(tokens);
        tokens = NULL;
    }
    if (builder.buffer != NULL) {
        free(builder.buffer);
        builder.buffer = NULL;
    }
    if (command != NULL) {
        thumbnailer_command_free(command);
        command = NULL;
    }

    return NULL;
}

/*
 * thumbnailer_is_evince_thumbnailer
 *
 * Detect whether the selected thumbnailer maps to evince-thumbnailer so
 * the stdout redirection workaround can be applied.
 *
 * Arguments:
 *     exec_line - Exec string parsed from the .thumbnailer file.
 *     tryexec   - optional TryExec value for additional matching.
 * Returns:
 *     1 when evince-thumbnailer is referenced, 0 otherwise.
 */
static int
thumbnailer_is_evince_thumbnailer(char const *exec_line,
                                  char const *tryexec)
{
    char const *needle;
    char const *basename;

    needle = "evince-thumbnailer";
    basename = NULL;

    if (exec_line != NULL && strstr(exec_line, needle) != NULL) {
        return 1;
    }

    if (tryexec != NULL) {
        basename = strrchr(tryexec, '/');
        if (basename != NULL) {
            basename += 1;
        } else {
            basename = tryexec;
        }
        if (strcmp(basename, needle) == 0) {
            return 1;
        }
        if (strstr(tryexec, needle) != NULL) {
            return 1;
        }
    }

    return 0;
}

/*
 * thumbnailer_build_evince_command
 *
 * Construct an argv sequence that streams evince-thumbnailer output to
 * stdout so downstream code can capture the PNG safely.
 *
 * Arguments:
 *     input_path - source document path.
 *     size       - numeric size hint forwarded to the -s option.
 * Returns:
 *     Newly allocated command or NULL on allocation failure.
 */
static struct thumbnailer_command *
thumbnailer_build_evince_command(char const *input_path,
                                 int size)
{
    struct thumbnailer_command *command;
    char size_text[16];
    size_t index;
    int written;

    command = NULL;
    index = 0;

    if (input_path == NULL) {
        return NULL;
    }

    command = malloc(sizeof(*command));
    if (command == NULL) {
        return NULL;
    }

    command->argc = 5;
    command->argv = malloc(sizeof(char *) * (command->argc + 1));
    if (command->argv == NULL) {
        thumbnailer_command_free(command);
        return NULL;
    }

    for (index = 0; index < command->argc + 1; ++index) {
        command->argv[index] = NULL;
    }

    written = sixel_compat_snprintf(size_text,
                                    sizeof(size_text),
                                    "%d",
                                    size);
    if (written < 0) {
        size_text[0] = '\0';
    } else if ((size_t)written >= sizeof(size_text)) {
        size_text[sizeof(size_text) - 1u] = '\0';
    }

    command->argv[0] = thumbnailer_strdup("evince-thumbnailer");
    command->argv[1] = thumbnailer_strdup("-s");
    command->argv[2] = thumbnailer_strdup(size_text);
    command->argv[3] = thumbnailer_strdup(input_path);
    command->argv[4] = thumbnailer_strdup("/dev/stdout");
    command->argv[5] = NULL;

    for (index = 0; index < command->argc; ++index) {
        if (command->argv[index] == NULL) {
            thumbnailer_command_free(command);
            return NULL;
        }
    }

    command->display = thumbnailer_command_format(command->argv,
                                                  command->argc);
    if (command->display == NULL) {
        thumbnailer_command_free(command);
        return NULL;
    }

    return command;
}

/*
 * thumbnailer_build_file_uri
 *
 * Convert a filesystem path into a percent-encoded file:// URI.
 *
 * Arguments:
 *     path - filesystem path; may be relative but will be resolved.
 * Returns:
 *     Newly allocated URI string or NULL on error.
 */
static char *
thumbnailer_build_file_uri(char const *path)
{
    char *resolved;
    size_t index;
    size_t length;
    size_t needed;
    char *uri;
    size_t position;
    char const hex[] = "0123456789ABCDEF";

    resolved = NULL;
    index = 0;
    length = 0;
    needed = 0;
    uri = NULL;
    position = 0;

    if (path == NULL) {
        return NULL;
    }

    resolved = thumbnailer_resolve_path(path);
    if (resolved == NULL) {
        return NULL;
    }

    length = strlen(resolved);
    needed = 7;
    for (index = 0; index < length; ++index) {
        unsigned char ch;

        ch = (unsigned char)resolved[index];
        if (isalnum(ch) || ch == '-' || ch == '_' ||
                ch == '.' || ch == '~' || ch == '/') {
            needed += 1;
        } else {
            needed += 3;
        }
    }

    uri = malloc(needed + 1);
    if (uri == NULL) {
        free(resolved);
        resolved = NULL;
        return NULL;
    }

    memcpy(uri, "file://", 7);
    position = 7;
    for (index = 0; index < length; ++index) {
        unsigned char ch;

        ch = (unsigned char)resolved[index];
        if (isalnum(ch) || ch == '-' || ch == '_' ||
                ch == '.' || ch == '~' || ch == '/') {
            uri[position++] = (char)ch;
        } else {
            uri[position++] = '%';
            uri[position++] = hex[(ch >> 4) & 0xF];
            uri[position++] = hex[ch & 0xF];
        }
    }
    uri[position] = '\0';

    free(resolved);
    resolved = NULL;

    return uri;
}

/*
 * thumbnailer_extract_mime_token
 *
 * Normalize file(1) output into a plain MIME identifier.
 *
 * GNU file with --mime-type may return:
 *     image/png
 * Solaris file with -i may return:
 *     /path/to/image.png: image/png; charset=binary
 *
 * This helper strips the optional "path:" prefix and any trailing
 * "; charset=..." suffix in-place.
 */
static char *
thumbnailer_extract_mime_token(char *text)
{
    char *token;
    char *scan;

    token = text;
    scan = text;

    while (*scan != '\0') {
        if (*scan == ':') {
            token = scan + 1;
            break;
        }
        ++scan;
    }

    token = thumbnailer_trim_left(token);
    scan = token;
    while (*scan != '\0' && *scan != ';' && !isspace((unsigned char)*scan)) {
        ++scan;
    }
    *scan = '\0';

    return token;
}

/*
 * thumbnailer_run_file
 *
 * Invoke file(1) and capture a single trimmed line of output.
 *
 * Arguments:
 *     path   - filesystem path forwarded to file(1).
 *     option - optional file(1) mode hint.  "--mime-type" selects
 *              build-time detected MIME probing while preserving legacy
 *              callers.
 * Returns:
 *     Newly allocated string trimmed of trailing whitespace or NULL on
 *     failure.
 */
char *
thumbnailer_run_file(char const *path, char const *option)
{
    int pipefd[2];
    pid_t pid;
    ssize_t bytes_read;
    char buffer[256];
    size_t total;
    int status;
    char *result;
    char *trimmed;
    int mime_mode;
    char const *mime_option;

    pipefd[0] = -1;
    pipefd[1] = -1;
    pid = (-1);
    bytes_read = 0;
    total = 0;
    status = 0;
    result = NULL;
    trimmed = NULL;
    mime_mode = 0;
    mime_option = SIXEL_FILE_MIME_OPTION;

    if (path == NULL) {
        return NULL;
    }

    if (option != NULL && strcmp(option, "--mime-type") == 0) {
        mime_mode = 1;
    }

    if (mime_option == NULL || mime_option[0] == '\0') {
        mime_option = "-i";
    }

    if (pipe(pipefd) < 0) {
        return NULL;
    }

    pid = fork();
    if (pid < 0) {
        sixel_compat_close(pipefd[0]);
        sixel_compat_close(pipefd[1]);
        return NULL;
    }

    if (pid == 0) {
        char const *argv[6];
        size_t arg_index;

        sixel_compat_close(pipefd[0]);
        if (dup2(pipefd[1], STDOUT_FILENO) < 0) {
            _exit(127);
        }
        sixel_compat_close(pipefd[1]);
        arg_index = 0u;
        argv[arg_index++] = "file";
        if (mime_mode) {
            argv[arg_index++] = mime_option;
        } else {
            argv[arg_index++] = "-b";
            if (option != NULL) {
                argv[arg_index++] = option;
            }
        }
        argv[arg_index++] = path;
        argv[arg_index] = NULL;
        execvp("file", (char * const *)argv);
        _exit(127);
    }

    sixel_compat_close(pipefd[1]);
    pipefd[1] = -1;
    total = 0;
    while ((bytes_read = read(pipefd[0], buffer + total,
                              sizeof(buffer) - total - 1)) > 0) {
        total += (size_t)bytes_read;
        if (total >= sizeof(buffer) - 1) {
            break;
        }
    }
    buffer[total] = '\0';
    sixel_compat_close(pipefd[0]);
    pipefd[0] = -1;

    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
        continue;
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        return NULL;
    }

    trimmed = thumbnailer_trim_left(buffer);
    thumbnailer_trim_right(trimmed);
    if (mime_mode) {
        trimmed = thumbnailer_extract_mime_token(trimmed);
    }
    if (trimmed[0] == '\0') {
        return NULL;
    }

    result = thumbnailer_strdup(trimmed);

    return result;
}

/*
 * thumbnailer_guess_content_type
 *
 * Obtain the MIME identifier for the supplied path using file(1).
 */
char *
thumbnailer_guess_content_type(char const *path)
{
    return thumbnailer_run_file(path, "--mime-type");
}

/*
 * Thumbnailer supervision overview:
 *
 *     +-------------------+    pipe(stderr)    +-------------------+
 *     | libsixel parent   | <----------------- | thumbnailer argv  |
 *     |  - polls stdout   |                   |  - renders PNG     |
 *     |  - polls stderr   | -----------------> |  - returns code   |
 *     |  - waits pid      |    pipe(stdout)    |                   |
 *     +-------------------+  posix_spawn/fork  +-------------------+
 *
 * Non-blocking pipes keep verbose thumbnailers from stalling the loop,
 * and argv arrays mean Exec templates never pass through /bin/sh.
 *
 * thumbnailer_spawn is responsible for preparing pipes, launching the
 * thumbnail helper, and streaming any emitted data back into libsixel.
 *
 *  - stderr is captured into stderr_output so verbose mode can replay the
 *    diagnostics without leaking them to non-verbose invocations.
 *  - stdout can optionally be redirected into a temporary file when
 *    thumbnailers insist on writing image data to the standard output stream.
 *  - All file descriptors are placed into non-blocking mode to avoid stalls
 *    while the parent waits for the child process to complete.
 * Arguments:
 *     command          - prepared argv array to execute.
 *     thumbnailer_name - friendly name used in log messages.
 *     log_prefix       - identifier describing the current pipeline step.
 *     capture_stdout   - non-zero to capture stdout into stdout_path.
 *     stdout_path      - file receiving stdout when capture_stdout != 0.
 * Returns:
 *     SIXEL_OK on success or a libsixel error code on failure.
 */
static SIXELSTATUS
thumbnailer_spawn(struct thumbnailer_command const *command,
                  char const *thumbnailer_name,
                  char const *log_prefix,
                  int capture_stdout,
                  char const *stdout_path)
{
    pid_t pid;
    int status_code;
    int wait_result;
    SIXELSTATUS status;
    char message[256];
    char errno_text[64];
    int stderr_pipe[2];
    int stdout_pipe[2];
    int stderr_pipe_created;
    int stdout_pipe_created;
    int flags;
    ssize_t read_result;
    ssize_t stdout_read_result;
    char stderr_buffer[256];
    char stdout_buffer[4096];
    char *stderr_output;
    size_t stderr_length;
    size_t stderr_capacity;
    int child_exited;
    int stderr_open;
    int stdout_open;
    int have_status;
    int fatal_error;
    int output_fd;
    size_t write_offset;
    ssize_t write_result;
    size_t to_write;
    char const *display_command;
    int written;
# if HAVE_POSIX_SPAWNP && !defined(__FreeBSD__) && !defined(__DragonFly__)
    posix_spawn_file_actions_t actions;
    int spawn_result;
# endif

    pid = (-1);
    status_code = 0;
    wait_result = 0;
    status = SIXEL_RUNTIME_ERROR;
    memset(message, 0, sizeof(message));
    stderr_pipe[0] = -1;
    stderr_pipe[1] = -1;
    stdout_pipe[0] = -1;
    stdout_pipe[1] = -1;
    stderr_pipe_created = 0;
    stdout_pipe_created = 0;
    flags = 0;
    read_result = 0;
    stdout_read_result = 0;
    stderr_output = NULL;
    stderr_length = 0;
    stderr_capacity = 0;
    child_exited = 0;
    stderr_open = 0;
    stdout_open = 0;
    have_status = 0;
    fatal_error = 0;
    output_fd = -1;
    write_offset = 0;
    write_result = 0;
    to_write = 0;
    display_command = NULL;
# if HAVE_POSIX_SPAWNP && !defined(__FreeBSD__) && !defined(__DragonFly__)
    spawn_result = 0;
# endif

    if (command == NULL || command->argv == NULL ||
            command->argv[0] == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (capture_stdout && stdout_path == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (capture_stdout) {
        output_fd = sixel_compat_open(stdout_path,
                         O_WRONLY | O_CREAT | O_TRUNC,
                         0600);
        if (output_fd < 0) {
            sixel_compat_strerror(errno,
                                  errno_text,
                                  sizeof(errno_text));
            written = sixel_compat_snprintf(message,
                                            sizeof(message),
                                            "%s: sixel_compat_open(%s) failed (%s).",
                                            log_prefix,
                                            stdout_path,
                                            errno_text);
            thumbnailer_message_finalize(message,
                                         sizeof(message),
                                         written);
            sixel_helper_set_additional_message(message);
            goto cleanup;
        }
    }

    /* stderr is collected even for successful runs so verbose users can see
     * the thumbnailer's own commentary.  Failing to set the pipe is not
     * fatal; we continue without stderr capture in that case.
     */
    if (pipe(stderr_pipe) == 0) {
        stderr_pipe_created = 1;
        stderr_open = 1;
    }

    if (capture_stdout) {
        if (pipe(stdout_pipe) != 0) {
            sixel_compat_strerror(errno,
                                  errno_text,
                                  sizeof(errno_text));
            written = sixel_compat_snprintf(
                message,
                sizeof(message),
                "%s: pipe() for stdout failed (%s).",
                log_prefix,
                errno_text);
            thumbnailer_message_finalize(message,
                                         sizeof(message),
                                         written);
            sixel_helper_set_additional_message(message);
            goto cleanup;
        }
        stdout_pipe_created = 1;
        stdout_open = 1;
    }

    display_command = (command->display != NULL) ?
            command->display : command->argv[0];
    loader_trace_message("%s: executing %s",
                         log_prefix,
                         display_command);

# if HAVE_POSIX_SPAWNP && !defined(__FreeBSD__) && !defined(__DragonFly__)
    if (posix_spawn_file_actions_init(&actions) != 0) {
        written = sixel_compat_snprintf(
            message,
            sizeof(message),
            "%s: posix_spawn_file_actions_init() failed.",
            log_prefix);
        thumbnailer_message_finalize(message,
                                     sizeof(message),
                                     written);
        sixel_helper_set_additional_message(message);
        goto cleanup;
    }
    if (stderr_pipe_created && stderr_pipe[1] >= 0) {
        (void)posix_spawn_file_actions_adddup2(&actions,
                                               stderr_pipe[1],
                                               STDERR_FILENO);
        (void)posix_spawn_file_actions_addclose(&actions,
                                                stderr_pipe[0]);
        (void)posix_spawn_file_actions_addclose(&actions,
                                                stderr_pipe[1]);
    }
    if (stdout_pipe_created && stdout_pipe[1] >= 0) {
        (void)posix_spawn_file_actions_adddup2(&actions,
                                               stdout_pipe[1],
                                               STDOUT_FILENO);
        (void)posix_spawn_file_actions_addclose(&actions,
                                                stdout_pipe[0]);
        (void)posix_spawn_file_actions_addclose(&actions,
                                                stdout_pipe[1]);
    }
    if (output_fd >= 0) {
        (void)posix_spawn_file_actions_addclose(&actions,
                                                output_fd);
    }
    spawn_result = posix_spawnp(&pid,
                                command->argv[0],
                                &actions,
                                NULL,
                                (char * const *)command->argv,
                                environ);
    posix_spawn_file_actions_destroy(&actions);
    if (spawn_result != 0) {
        sixel_compat_strerror(spawn_result,
                              errno_text,
                              sizeof(errno_text));
        written = sixel_compat_snprintf(message,
                                        sizeof(message),
                                        "%s: posix_spawnp() failed (%s).",
                                        log_prefix,
                                        errno_text);
        thumbnailer_message_finalize(message,
                                     sizeof(message),
                                     written);
        sixel_helper_set_additional_message(message);
        goto cleanup;
    }
# else
    /*
     * FreeBSD falls back to fork()/execvp() so that the shared library does
     * not depend on an `environ` symbol that libc leaves undefined when
     * linking with --no-undefined.
     */
    pid = fork();
    if (pid < 0) {
        sixel_compat_strerror(errno,
                              errno_text,
                              sizeof(errno_text));
        written = sixel_compat_snprintf(message,
                                        sizeof(message),
                                        "%s: fork() failed (%s).",
                                        log_prefix,
                                        errno_text);
        thumbnailer_message_finalize(message,
                                     sizeof(message),
                                     written);
        sixel_helper_set_additional_message(message);
        goto cleanup;
    }
    if (pid == 0) {
        if (stderr_pipe_created && stderr_pipe[1] >= 0) {
            if (dup2(stderr_pipe[1], STDERR_FILENO) < 0) {
                _exit(127);
            }
        }
        if (stdout_pipe_created && stdout_pipe[1] >= 0) {
            if (dup2(stdout_pipe[1], STDOUT_FILENO) < 0) {
                _exit(127);
            }
        }
        if (stderr_pipe[0] >= 0) {
            sixel_compat_close(stderr_pipe[0]);
        }
        if (stderr_pipe[1] >= 0) {
            sixel_compat_close(stderr_pipe[1]);
        }
        if (stdout_pipe[0] >= 0) {
            sixel_compat_close(stdout_pipe[0]);
        }
        if (stdout_pipe[1] >= 0) {
            sixel_compat_close(stdout_pipe[1]);
        }
        if (output_fd >= 0) {
            sixel_compat_close(output_fd);
        }
        execvp(command->argv[0], (char * const *)command->argv);
        _exit(127);
    }
# endif

    loader_trace_message("%s: forked child pid=%ld",
                         log_prefix,
                         (long)pid);

    if (stderr_pipe_created && stderr_pipe[1] >= 0) {
        sixel_compat_close(stderr_pipe[1]);
        stderr_pipe[1] = -1;
    }
    if (stdout_pipe_created && stdout_pipe[1] >= 0) {
        sixel_compat_close(stdout_pipe[1]);
        stdout_pipe[1] = -1;
    }

    if (stderr_pipe_created && stderr_pipe[0] >= 0) {
        flags = fcntl(stderr_pipe[0], F_GETFL, 0);
        if (flags >= 0) {
            (void)fcntl(stderr_pipe[0],
                        F_SETFL,
                        flags | O_NONBLOCK);
        }
    }
    if (stdout_pipe_created && stdout_pipe[0] >= 0) {
        flags = fcntl(stdout_pipe[0], F_GETFL, 0);
        if (flags >= 0) {
            (void)fcntl(stdout_pipe[0],
                        F_SETFL,
                        flags | O_NONBLOCK);
        }
    }

    /* The monitoring loop drains stderr/stdout as long as any descriptor is
     * open.  Non-blocking reads ensure the parent does not deadlock if the
     * child process stalls or writes data in bursts.
     */
    while (!child_exited || stderr_open || stdout_open) {
        if (stderr_pipe_created && stderr_open) {
            read_result = read(stderr_pipe[0],
                               stderr_buffer,
                               (ssize_t)sizeof(stderr_buffer));
            if (read_result > 0) {
                if (stderr_length + (size_t)read_result + 1 >
                        stderr_capacity) {
                    size_t new_capacity;
                    char *new_output;

                    new_capacity = stderr_capacity;
                    if (new_capacity == 0) {
                        new_capacity = 256;
                    }
                    while (stderr_length + (size_t)read_result + 1 >
                            new_capacity) {
                        new_capacity *= 2U;
                    }
                    new_output = realloc(stderr_output, new_capacity);
                    if (new_output == NULL) {
                        free(stderr_output);
                        stderr_output = NULL;
                        stderr_capacity = 0;
                        stderr_length = 0;
                        stderr_open = 0;
                        if (stderr_pipe[0] >= 0) {
                            sixel_compat_close(stderr_pipe[0]);
                            stderr_pipe[0] = -1;
                        }
                        stderr_pipe_created = 0;
                        written = sixel_compat_snprintf(message,
                                                        sizeof(message),
                                                        "%s: realloc() failed.",
                                                        log_prefix);
                        thumbnailer_message_finalize(message,
                                                     sizeof(message),
                                                     written);
                        sixel_helper_set_additional_message(message);
                        status = SIXEL_BAD_ALLOCATION;
                        fatal_error = 1;
                        break;
                    }
                    stderr_output = new_output;
                    stderr_capacity = new_capacity;
                }
                memcpy(stderr_output + stderr_length,
                       stderr_buffer,
                       (size_t)read_result);
                stderr_length += (size_t)read_result;
                stderr_output[stderr_length] = '\0';
            } else if (read_result == 0) {
                stderr_open = 0;
                if (stderr_pipe[0] >= 0) {
                    sixel_compat_close(stderr_pipe[0]);
                    stderr_pipe[0] = -1;
                }
                stderr_pipe_created = 0;
            } else if (errno == EINTR) {
                /* retry */
            } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                /* no data */
            } else {
                sixel_compat_strerror(errno,
                                      errno_text,
                                      sizeof(errno_text));
                written = sixel_compat_snprintf(message,
                                                sizeof(message),
                                                "%s: read() failed (%s).",
                                                log_prefix,
                                                errno_text);
                thumbnailer_message_finalize(message,
                                             sizeof(message),
                                             written);
                sixel_helper_set_additional_message(message);
                stderr_open = 0;
                if (stderr_pipe[0] >= 0) {
                    sixel_compat_close(stderr_pipe[0]);
                    stderr_pipe[0] = -1;
                }
                stderr_pipe_created = 0;
            }
        }

        if (stdout_pipe_created && stdout_open) {
            stdout_read_result = read(stdout_pipe[0],
                                      stdout_buffer,
                                      (ssize_t)sizeof(stdout_buffer));
            if (stdout_read_result > 0) {
                write_offset = 0;
                while (write_offset < (size_t)stdout_read_result) {
                    to_write = (size_t)stdout_read_result - write_offset;
                    write_result = sixel_compat_write(output_fd,
                                          stdout_buffer + write_offset,
                                          to_write);
                    if (write_result < 0) {
                        if (errno == EINTR) {
                            continue;
                        }
                    sixel_compat_strerror(errno,
                                          errno_text,
                                          sizeof(errno_text));
                    written = sixel_compat_snprintf(message,
                                                    sizeof(message),
                                                    "%s: sixel_compat_write() failed (%s).",
                                                    log_prefix,
                                                    errno_text);
                    thumbnailer_message_finalize(message,
                                                 sizeof(message),
                                                 written);
                    sixel_helper_set_additional_message(message);
                    stdout_open = 0;
                    fatal_error = 1;
                    break;
                }
                    write_offset += (size_t)write_result;
                }
            } else if (stdout_read_result == 0) {
                stdout_open = 0;
                if (stdout_pipe[0] >= 0) {
                    sixel_compat_close(stdout_pipe[0]);
                    stdout_pipe[0] = -1;
                }
                stdout_pipe_created = 0;
            } else if (errno == EINTR) {
                /* retry */
            } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                /* no data */
            } else {
                sixel_compat_strerror(errno,
                                      errno_text,
                                      sizeof(errno_text));
                written = sixel_compat_snprintf(message,
                                                sizeof(message),
                                                "%s: read() failed (%s).",
                                                log_prefix,
                                                errno_text);
                thumbnailer_message_finalize(message,
                                             sizeof(message),
                                             written);
                sixel_helper_set_additional_message(message);
                stdout_open = 0;
                if (stdout_pipe[0] >= 0) {
                    sixel_compat_close(stdout_pipe[0]);
                    stdout_pipe[0] = -1;
                }
                stdout_pipe_created = 0;
            }
        }

        if (!child_exited) {
            wait_result = waitpid(pid, &status_code, WNOHANG);
            if (wait_result > 0) {
                child_exited = 1;
                have_status = 1;
            } else if (wait_result == 0) {
                /* child running */
            } else if (errno != EINTR) {
            sixel_compat_strerror(errno,
                                  errno_text,
                                  sizeof(errno_text));
            written = sixel_compat_snprintf(message,
                                            sizeof(message),
                                            "%s: waitpid() failed (%s).",
                                            log_prefix,
                                            errno_text);
            thumbnailer_message_finalize(message,
                                         sizeof(message),
                                         written);
            sixel_helper_set_additional_message(message);
            status = SIXEL_RUNTIME_ERROR;
            fatal_error = 1;
            break;
        }
        }

        if (!child_exited || stderr_open || stdout_open) {
            /*
             * Brief sleep prevents busy waiting while periodically polling
             * child processes and pipe handles.
             */
            sixel_sleep(THUMBNAILER_POLL_SLEEP_USEC);
        }
    }

    if (!child_exited) {
        do {
            wait_result = waitpid(pid, &status_code, 0);
        } while (wait_result < 0 && errno == EINTR);
        if (wait_result < 0) {
        sixel_compat_strerror(errno,
                              errno_text,
                              sizeof(errno_text));
        written = sixel_compat_snprintf(message,
                                        sizeof(message),
                                        "%s: waitpid() failed (%s).",
                                        log_prefix,
                                        errno_text);
        thumbnailer_message_finalize(message,
                                     sizeof(message),
                                     written);
        sixel_helper_set_additional_message(message);
        status = SIXEL_RUNTIME_ERROR;
        goto cleanup;
    }
        have_status = 1;
    }

    if (!have_status) {
        written = sixel_compat_snprintf(message,
                                        sizeof(message),
                                        "%s: waitpid() failed (no status).",
                                        log_prefix);
        thumbnailer_message_finalize(message,
                                     sizeof(message),
                                     written);
        sixel_helper_set_additional_message(message);
        status = SIXEL_RUNTIME_ERROR;
        goto cleanup;
    }

    if (!fatal_error) {
        if (WIFEXITED(status_code) && WEXITSTATUS(status_code) == 0) {
            status = SIXEL_OK;
            loader_trace_message("%s: child pid=%ld exited successfully",
                                 log_prefix,
                                 (long)pid);
        } else if (WIFEXITED(status_code)) {
            written = sixel_compat_snprintf(message,
                                            sizeof(message),
                                            "%s: %s exited with status %d.",
                                            log_prefix,
                                            (thumbnailer_name != NULL) ?
                                            thumbnailer_name :
                                            "thumbnailer",
                                            WEXITSTATUS(status_code));
            thumbnailer_message_finalize(message,
                                         sizeof(message),
                                         written);
            sixel_helper_set_additional_message(message);
            status = SIXEL_RUNTIME_ERROR;
        } else if (WIFSIGNALED(status_code)) {
            written = sixel_compat_snprintf(message,
                                            sizeof(message),
                                            "%s: %s terminated by signal %d.",
                                            log_prefix,
                                            (thumbnailer_name != NULL) ?
                                            thumbnailer_name :
                                            "thumbnailer",
                                            WTERMSIG(status_code));
            thumbnailer_message_finalize(message,
                                         sizeof(message),
                                         written);
            sixel_helper_set_additional_message(message);
            status = SIXEL_RUNTIME_ERROR;
        } else {
            written = sixel_compat_snprintf(message,
                                            sizeof(message),
                                            "%s: %s exited abnormally.",
                                            log_prefix,
                                            (thumbnailer_name != NULL) ?
                                            thumbnailer_name :
                                            "thumbnailer");
            thumbnailer_message_finalize(message,
                                         sizeof(message),
                                         written);
            sixel_helper_set_additional_message(message);
            status = SIXEL_RUNTIME_ERROR;
        }
    }

cleanup:
    if (stderr_output != NULL && loader_trace_is_enabled() &&
            stderr_length > 0) {
        loader_trace_message("%s: stderr:\n%s",
                             log_prefix,
                             stderr_output);
    }

    if (stderr_pipe[0] >= 0) {
        sixel_compat_close(stderr_pipe[0]);
        stderr_pipe[0] = -1;
    }
    if (stderr_pipe[1] >= 0) {
        sixel_compat_close(stderr_pipe[1]);
        stderr_pipe[1] = -1;
    }
    if (stdout_pipe[0] >= 0) {
        sixel_compat_close(stdout_pipe[0]);
        stdout_pipe[0] = -1;
    }
    if (stdout_pipe[1] >= 0) {
        sixel_compat_close(stdout_pipe[1]);
        stdout_pipe[1] = -1;
    }
    if (output_fd >= 0) {
        sixel_compat_close(output_fd);
        output_fd = -1;
    }
    /* stderr_output accumulates all diagnostic text, so release it even when
     * verbose tracing is disabled.
     */
    free(stderr_output);

    return status;
}



/*
 * load_with_gnome_thumbnailer
 *
 * Drive the FreeDesktop thumbnailer pipeline and then decode the PNG
 * result using the built-in loader.
 *
 * GNOME thumbnail workflow overview:
 *
 *     +------------+    +-------------------+    +----------------+
 *     | source URI | -> | .thumbnailer Exec | -> | PNG thumbnail  |
 *     +------------+    +-------------------+    +----------------+
 *             |                    |                        |
 *             |                    v                        v
 *             |           spawn via /bin/sh         load_with_builtin()
 *             v
 *     file --mime-type
 *
 * Each step logs verbose breadcrumbs so integrators can diagnose which
 * thumbnailer matched, how the command was prepared, and why fallbacks
 * were selected.
 *
 * Arguments:
 *     pchunk        - source chunk representing the original document.
 *     fstatic       - image static-ness flag.
 *     fuse_palette  - palette usage flag.
 *     reqcolors     - requested colour count.
 *     bgcolor       - background colour override.
 *     loop_control  - animation loop control flag.
 *     start_frame_no_set - whether a start frame override is active.
 *     start_frame_no - start frame override value when set.
 *     fn_load       - downstream decoder callback.
 *     context       - user context forwarded to fn_load.
 * Returns:
 *     SIXEL_OK on success or libsixel error code on failure.
 */
SIXELSTATUS
load_with_gnome_thumbnailer(
    sixel_chunk_t const       /* in */     *pchunk,
    int                       /* in */     fstatic,
    int                       /* in */     fuse_palette,
    int                       /* in */     reqcolors,
    unsigned char             /* in */     *bgcolor,
    int                       /* in */     loop_control,
    int                       /* in */     start_frame_no_set,
    int                       /* in */     start_frame_no,
    sixel_load_image_function /* in */     fn_load,
    void                      /* in/out */ *context)
{
    SIXELSTATUS status;
    sixel_chunk_t *thumb_chunk;
    char template_path[] = "/tmp/libsixel-thumb-XXXXXX";
    char *png_path;
    size_t path_length;
    struct thumbnailer_string_list *directories;
    size_t dir_index;
    DIR *dir;
    struct dirent *entry;
    char *thumbnailer_path;
    struct thumbnailer_entry info;
    char *content_type;
    char *input_uri;
    struct thumbnailer_command *command;
    struct thumbnailer_command *evince_command;
    int executed;
    int command_success;
    int requested_size;
    char const *log_prefix;
    int fd;
    int written;

    (void)start_frame_no_set;
    (void)start_frame_no;

    loader_thumbnailer_initialize_size_hint();

    status = SIXEL_FALSE;
    thumb_chunk = NULL;
    png_path = NULL;
    path_length = 0;
    fd = -1;
    directories = NULL;
    dir_index = 0;
    dir = NULL;
    entry = NULL;
    thumbnailer_path = NULL;
    content_type = NULL;
    input_uri = NULL;
    command = NULL;
    evince_command = NULL;
    executed = 0;
    command_success = 0;
    log_prefix = "load_with_gnome_thumbnailer";
    requested_size = loader_thumbnailer_get_size_hint();
    if (requested_size <= 0) {
        requested_size = SIXEL_THUMBNAILER_DEFAULT_SIZE;
    }

    loader_trace_message("%s: thumbnail size hint=%d",
                         log_prefix,
                         requested_size);

    thumbnailer_entry_init(&info);

    if (pchunk->source_path == NULL) {
        sixel_helper_set_additional_message(
            "load_with_gnome_thumbnailer: source path is unavailable.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    if (sixel_compat_mktemp(template_path, sizeof(template_path)) != 0) {
        sixel_helper_set_additional_message(
            "load_with_gnome_thumbnailer: mktemp() failed.");
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }
    fd = sixel_compat_open(template_path, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (fd < 0) {
        sixel_helper_set_additional_message(
            "load_with_gnome_thumbnailer: open() failed for temp file.");
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }
    sixel_compat_close(fd);
    fd = -1;

    path_length = strlen(template_path) + 5;
    png_path = malloc(path_length);
    if (png_path == NULL) {
        sixel_helper_set_additional_message(
            "load_with_gnome_thumbnailer: malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        sixel_compat_unlink(template_path);
        goto end;
    }
    written = sixel_compat_snprintf(png_path,
                                    path_length,
                                    "%s.png",
                                    template_path);
    thumbnailer_message_finalize(png_path,
                                 path_length,
                                 written);
    if (rename(template_path, png_path) != 0) {
        sixel_helper_set_additional_message(
            "load_with_gnome_thumbnailer: rename() failed.");
        status = SIXEL_RUNTIME_ERROR;
        sixel_compat_unlink(template_path);
        goto end;
    }

    content_type = thumbnailer_guess_content_type(pchunk->source_path);
    input_uri = thumbnailer_build_file_uri(pchunk->source_path);

    loader_trace_message("%s: detected MIME type %s for %s",
                         log_prefix,
                         (content_type != NULL) ? content_type :
                         "(unknown)",
                         pchunk->source_path);

    directories = thumbnailer_collect_directories();
    if (directories == NULL) {
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }

    /* Iterate through every configured thumbnailer directory so we honour
     * overrides in $HOME as well as desktop environment defaults discovered
     * through XDG_DATA_DIRS.
     */
    for (dir_index = 0; dir_index < directories->length; ++dir_index) {
        loader_trace_message("%s: checking thumbnailers in %s",
                             log_prefix,
                             directories->items[dir_index]);

        dir = opendir(directories->items[dir_index]);
        if (dir == NULL) {
            continue;
        }
        while ((entry = readdir(dir)) != NULL) {
            thumbnailer_entry_clear(&info);
            thumbnailer_entry_init(&info);
            size_t name_length;

            name_length = strlen(entry->d_name);
            if (name_length < 12 ||
                    strcmp(entry->d_name + name_length - 12,
                           ".thumbnailer") != 0) {
                continue;
            }
            thumbnailer_path = thumbnailer_join_paths(
                directories->items[dir_index],
                entry->d_name);
            if (thumbnailer_path == NULL) {
                continue;
            }
            if (!thumbnailer_parse_file(thumbnailer_path, &info)) {
                free(thumbnailer_path);
                thumbnailer_path = NULL;
                continue;
            }
            free(thumbnailer_path);
            thumbnailer_path = NULL;
            loader_trace_message(
                "%s: parsed %s (TryExec=%s)",
                log_prefix,
                entry->d_name,
                (info.tryexec != NULL) ? info.tryexec : "(none)");
            if (content_type == NULL) {
                continue;
            }
            if (!thumbnailer_has_tryexec(info.tryexec)) {
                loader_trace_message("%s: skipping %s (TryExec missing)",
                                     log_prefix,
                                     entry->d_name);
                continue;
            }
            if (!thumbnailer_supports_mime(&info, content_type)) {
                loader_trace_message("%s: %s does not support %s",
                                     log_prefix,
                                     entry->d_name,
                                     content_type);
                continue;
            }
            if (info.exec_line == NULL) {
                continue;
            }
            loader_trace_message("%s: %s supports %s with Exec=\"%s\"",
                                 log_prefix,
                                 entry->d_name,
                                 content_type,
                                 info.exec_line);
            loader_trace_message("%s: preparing %s for %s",
                                 log_prefix,
                                 entry->d_name,
                                 content_type);
            command = thumbnailer_build_command(info.exec_line,
                                                pchunk->source_path,
                                                input_uri,
                                                png_path,
                                                requested_size,
                                                content_type);
            if (command == NULL) {
                continue;
            }
            if (thumbnailer_is_evince_thumbnailer(info.exec_line,
                                                  info.tryexec)) {
                loader_trace_message(
                    "%s: applying evince-thumbnailer stdout workaround",
                    log_prefix);
                /* evince-thumbnailer fails when passed an output path.
                 * Redirect stdout and copy the stream instead.
                 */
                evince_command = thumbnailer_build_evince_command(
                    pchunk->source_path,
                    requested_size);
                if (evince_command == NULL) {
                    thumbnailer_command_free(command);
                    command = NULL;
                    continue;
                }
                thumbnailer_command_free(command);
                command = evince_command;
                evince_command = NULL;
                sixel_compat_unlink(png_path);
                status = thumbnailer_spawn(command,
                                           entry->d_name,
                                           log_prefix,
                                           1,
                                           png_path);
            } else {
                sixel_compat_unlink(png_path);
                status = thumbnailer_spawn(command,
                                           entry->d_name,
                                           log_prefix,
                                           0,
                                           NULL);
            }
            thumbnailer_command_free(command);
            command = NULL;
            executed = 1;
            if (SIXEL_SUCCEEDED(status)) {
                command_success = 1;
                loader_trace_message("%s: %s produced %s",
                                     log_prefix,
                                     entry->d_name,
                                     png_path);
                break;
            }
        }
        closedir(dir);
        dir = NULL;
        if (command_success) {
            break;
        }
    }

    if (!command_success) {
        loader_trace_message("%s: falling back to gdk-pixbuf-thumbnailer",
                             log_prefix);
        sixel_compat_unlink(png_path);
        command = thumbnailer_build_command(
            "gdk-pixbuf-thumbnailer --size=%s %i %o",
            pchunk->source_path,
            input_uri,
            png_path,
            requested_size,
            content_type);
        if (command != NULL) {
            sixel_compat_unlink(png_path);
            status = thumbnailer_spawn(command,
                                       "gdk-pixbuf-thumbnailer",
                                       log_prefix,
                                       0,
                                       NULL);
            thumbnailer_command_free(command);
            command = NULL;
            if (SIXEL_FAILED(status)) {
                goto end;
            }
            executed = 1;
            command_success = 1;
            loader_trace_message("%s: gdk-pixbuf-thumbnailer produced %s",
                                 log_prefix,
                                 png_path);
        }
    }

    if (!executed) {
        sixel_helper_set_additional_message(
            "load_with_gnome_thumbnailer: no thumbnailer available.");
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }

    status = sixel_chunk_new(&thumb_chunk,
                             png_path,
                             0,
                             NULL,
                             pchunk->allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = load_with_builtin(thumb_chunk,
                               fstatic,
                               fuse_palette,
                               reqcolors,
                               bgcolor,
                               loop_control,
                               start_frame_no_set,
                               start_frame_no,
                               fn_load,
                               context);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = SIXEL_OK;

end:
    if (command != NULL) {
        thumbnailer_command_free(command);
        command = NULL;
    }
    if (evince_command != NULL) {
        thumbnailer_command_free(evince_command);
        evince_command = NULL;
    }
    if (thumb_chunk != NULL) {
        sixel_chunk_destroy(thumb_chunk);
        thumb_chunk = NULL;
    }
    if (png_path != NULL) {
        sixel_compat_unlink(png_path);
        free(png_path);
        png_path = NULL;
    }
    if (fd >= 0) {
        sixel_compat_close(fd);
        fd = -1;
    }
    if (directories != NULL) {
        thumbnailer_string_list_free(directories);
        directories = NULL;
    }
    if (dir != NULL) {
        closedir(dir);
        dir = NULL;
    }
    thumbnailer_entry_clear(&info);
    free(content_type);
    content_type = NULL;
    free(input_uri);
    input_uri = NULL;

    return status;
}

#endif  /* HAVE_FREEDESKTOP_THUMBNAILING */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
