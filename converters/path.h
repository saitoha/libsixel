/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
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

#ifndef IMG2SIXEL_PATH_H
#define IMG2SIXEL_PATH_H

#include <stddef.h>

/*
 * Convert a path to a libc-friendly representation for the current build.
 *
 * Overview (conversion only happens when the build expects a different path
 * style, while still accepting mixed inputs on Windows):
 *
 *   +----------------------+-------------------------+---------------------+
 *   | Build                | Input                   | Output              |
 *   +----------------------+-------------------------+---------------------+
 *   | MSVC / MinGW         | /c/...                  | c:/...              |
 *   | MSVC / MinGW         | /cygdrive/c/...          | c:/...              |
 *   | MSYS (msys-1.0.dll)  | C:\\... or C:/...        | /c/...              |
 *   | MSYS (msys-1.0.dll)  | /cygdrive/c/...          | /c/...              |
 *   | Cygwin               | C:\\... or C:/...        | /cygdrive/c/...     |
 *   | Cygwin               | /c/...                  | /cygdrive/c/...     |
 *   | Any build            | ./a/b or a/b             | (unchanged)         |
 *   +----------------------+-------------------------+---------------------+
 *
 * The helper never allocates. Call img2sixel_path_to_libc_buffer_size() first
 * to determine if conversion is needed. If the returned size is 0, the path is
 * already compatible and the original pointer can be used. Otherwise, allocate
 * a buffer of that size and pass it to img2sixel_path_to_libc().
 */
size_t img2sixel_path_to_libc_buffer_size(char const *path);

char const *img2sixel_path_to_libc(char const *path,
                                   char *buffer,
                                   size_t buffer_size);

#endif  /* IMG2SIXEL_PATH_H */
