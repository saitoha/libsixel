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

#ifndef LIBSIXEL_STDIO_STUB_H
#define LIBSIXEL_STDIO_STUB_H

#include <stdio.h>

#if defined(_WIN32)
/*
 * The Windows CRT already exports _fileno with the correct dllimport
 * decoration. Redeclaring fileno here triggers -Winconsistent-dllimport
 * under clang-cl, so we always prefer the vendor prototype.
 */
# define sixel_fileno(stream) _fileno(stream)
#elif defined(HAVE_FILENO) && HAVE_FILENO
/*
 * OpenBSD exposes fileno as a macro, so we avoid redeclaring it to keep
 * the compiler happy. Other platforms may need an explicit prototype in
 * this header to make sixel_fileno available without relying on an
 * implicit declaration.
 */
# ifndef fileno
int fileno(FILE *);
# endif
# define sixel_fileno(stream) fileno(stream)
#elif defined(HAVE__FILENO) && HAVE__FILENO
/*
 * Prefer the standard fileno when available. Falling back to _fileno is
 * still required for some Windows environments, but using it on systems
 * where both symbols are present (or misdetected during cross builds)
 * causes implicit declaration errors.
 */
# define sixel_fileno(stream) _fileno(stream)
#else
static int
sixel_fileno_stub(FILE *fp)
{
    return -1;
}

# define sixel_fileno(stream) sixel_fileno_stub(stream)
#endif  /* defined(HAVE__FILENO) && HAVE__FILENO */

#endif  /* LIBSIXEL_STDIO_STUB_H */

/* EOF */
