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

#ifndef IMG2SIXEL_COMPLETION_UTILS_H
#define IMG2SIXEL_COMPLETION_UTILS_H

#include <sixel.h>

#if HAVE_STDDEF_H
# include <stddef.h>
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

/*
 * Completion source selection is process policy owned by img2sixel.  Keep its
 * storage beside the converter instead of adding an application-specific
 * target to the library option registry.
 */
typedef struct img2sixel_completion_policy {
    char const *bash_path;
    int bash_path_override;
    char const *zsh_path;
    int zsh_path_override;
    char const *directory;
    int directory_override;
    char const *home;
    int home_override;
} img2sixel_completion_policy_t;

img2sixel_completion_policy_t *img2sixel_completion_policy_new(void);
void img2sixel_completion_policy_free(
    img2sixel_completion_policy_t *policy);
SIXELSTATUS img2sixel_completion_policy_apply(
    img2sixel_completion_policy_t *policy,
    char const *argument,
    char *diagnostic,
    size_t diagnostic_size);

int read_entire_file(const char *path, char **buf, size_t *len);
int write_atomic(const char *dst_path, const void *buf, size_t len,
                 mode_t mode);
int ensure_dir_p(const char *path, mode_t mode);
int files_equal(const char *path, const void *buf, size_t len);
int ensure_line_in_file(const char *path, const char *line);
int get_completion_text(
    const char *shell,
    img2sixel_completion_policy_t const *policy,
    char **out,
    size_t *len);
int img2sixel_handle_completion_option(
    int option,
    const char *value,
    img2sixel_completion_policy_t const *policy,
    int *exit_code);
int img2sixel_handle_completion_cli(
    int argc,
    char **argv,
    img2sixel_completion_policy_t const *policy,
    int *exit_code);

#endif  /* IMG2SIXEL_COMPLETION_UTILS_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
