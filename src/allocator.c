/*
 * Copyright (c) 2014,2015 Hayaki Saito
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

#include <stdlib.h>
#include "config.h"

#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif  /* HAVE_SYS_TYPES_H */

#if HAVE_ERRNO_H
# include <errno.h>
#endif  /* HAVE_ERRNO_H */

#if HAVE_MEMORY_H
# include <memory.h>
#endif  /* HAVE_MEMORY_H */

#include "allocator.h"


SIXELSTATUS
sixel_allocator_new(
    sixel_allocator_t   /* out */ **ppallocator,
    sixel_malloc_t      /* in */  fn_malloc,
    sixel_realloc_t     /* in */  fn_realloc,
    sixel_free_t        /* in */  fn_free)
{
    SIXELSTATUS status = SIXEL_FALSE;

    if (ppallocator == NULL) {
        sixel_helper_set_additional_message(
            "sixel_allocator_new: given argument ppallocator is null.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    if (fn_malloc == NULL) {
        sixel_helper_set_additional_message(
            "sixel_allocator_new: given argument fn_malloc is null.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    if (fn_realloc == NULL) {
        sixel_helper_set_additional_message(
            "sixel_allocator_new: given argument fn_realloc is null.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    if (fn_free == NULL) {
        sixel_helper_set_additional_message(
            "sixel_allocator_new: given argument fn_free is null.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    *ppallocator = fn_malloc(sizeof(sixel_allocator_t));
    if (*ppallocator == NULL) {
        sixel_helper_set_additional_message(
            "sixel_allocator_new: fn_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    (*ppallocator)->ref         = 1;
    (*ppallocator)->fn_malloc   = fn_malloc;
    (*ppallocator)->fn_realloc  = fn_realloc;
    (*ppallocator)->fn_free     = fn_free;

    status = SIXEL_OK;

end:
    return status;
}


static void
sixel_allocator_destroy(
    sixel_allocator_t   /* in */ *allocator)   /* allocator object to
                                                  be destroyed */
{
    allocator->fn_free(allocator);
}


SIXELAPI void
sixel_allocator_ref(sixel_allocator_t *allocator)
{
    /* TODO: be thread safe */
    ++allocator->ref;
}


SIXELAPI void
sixel_allocator_unref(sixel_allocator_t *allocator)
{
    /* TODO: be thread safe */
    if (allocator != NULL && --allocator->ref == 0) {
        sixel_allocator_destroy(allocator);
    }
}


#if HAVE_TESTS
void *
sixel_bad_malloc(size_t n)
{
    (void) n;

    return NULL;
}


void *
sixel_bad_realloc(void *p, size_t n)
{
    (void) p;
    (void) n;

    return NULL;
}
#endif  /* HAVE_TESTS */

#if 0
int
rpl_posix_memalign(void **memptr, size_t alignment, size_t size)
{
#if HAVE_POSIX_MEMALIGN
    return posix_memalign(memptr, alignment, size);
#elif HAVE_ALIGNED_ALLOC
    *memptr = aligned_alloc(alignment, size);
    return *memptr ? 0: ENOMEM;
#elif HAVE_MEMALIGN
    *memptr = memalign(alignment, size);
    return *memptr ? 0: ENOMEM;
#elif HAVE__ALIGNED_MALLOC
    return _aligned_malloc(size, alignment);
#else
# error
#endif /* _MSC_VER */
}
#endif


#if HAVE_TESTS
static int
test1(void)
{
    int nret = EXIT_FAILURE;
    SIXELSTATUS status;
    sixel_allocator_t *allocator = NULL;

    status = sixel_allocator_new(NULL, malloc, realloc, free);
    if (status != SIXEL_BAD_ARGUMENT) {
        goto error;
    }

    status = sixel_allocator_new(&allocator, NULL, realloc, free);
    if (status != SIXEL_BAD_ARGUMENT) {
        goto error;
    }

    status = sixel_allocator_new(&allocator, malloc, NULL, free);
    if (status != SIXEL_BAD_ARGUMENT) {
        goto error;
    }

    status = sixel_allocator_new(&allocator, malloc, realloc, NULL);
    if (status != SIXEL_BAD_ARGUMENT) {
        goto error;
    }

    nret = EXIT_SUCCESS;

error:
    return nret;
}


int
sixel_allocator_tests_main(void)
{
    int nret = EXIT_FAILURE;
    size_t i;
    typedef int (* testcase)(void);

    static testcase const testcases[] = {
        test1,
    };

    for (i = 0; i < sizeof(testcases) / sizeof(testcase); ++i) {
        nret = testcases[i]();
        if (nret != EXIT_SUCCESS) {
            goto error;
        }
    }

    nret = EXIT_SUCCESS;

error:
    return nret;
}
#endif  /* HAVE_TESTS */


/* Hello emacs, -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set expandtab ts=4 : */
/* EOF */
