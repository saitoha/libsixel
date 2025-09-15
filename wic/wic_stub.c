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

#include <objbase.h>

#define INITGUID
#include "wic_stub.h"

/* custom malloc */
void *
wic_malloc(size_t size)
{
    return CoTaskMemAlloc(size);
}

/* custom realloc */
void *
wic_realloc(void* ptr, size_t new_size)
{
    return CoTaskMemRealloc(ptr, new_size);
}

/* custom calloc */
void *
wic_calloc(size_t count, size_t size)
{
    size_t total;
    void *p;

    total = count * size;
    p = CoTaskMemAlloc(total);
    if (p) {
        memset(p, 0, total);
    }
    return p;
}

/* custom free */
void
wic_free(void* ptr)
{
    if (ptr) {
        CoTaskMemFree(ptr);
    }
}
