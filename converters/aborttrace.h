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
 * The CLI tools install a SIGABRT handler when the signal keeps the default
 * disposition.  The handler prints a short stack trace to stderr before
 * handing the signal back to the default action so sanitizers retain control.
 *
 */

#ifndef LIBSIXEL_CONVERTERS_ABORTTRACE_H
#define LIBSIXEL_CONVERTERS_ABORTTRACE_H

/* Internal helper that installs a SIGABRT handler for CLI tools when the
 * signal is currently unhandled.  The handler prints a trimmed stack trace
 * before letting the default disposition terminate the process.
 */

#ifdef __cplusplus
extern "C" {
#endif

void
sixel_aborttrace_install_if_unhandled(void);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_CONVERTERS_ABORTTRACE_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
