/* SPDX-License-Identifier: MIT */
#ifndef LSQA_ENVIRONMENT_H
#define LSQA_ENVIRONMENT_H

#include <stdlib.h>
#include <string.h>

/* Keep assessment independent of private core compatibility headers.
 * Every caller owns its snapshot and releases it with free(). MSVC exposes
 * _dupenv_s in stdlib.h; do not fall back to its deprecated getenv when a
 * feature probe is absent in a standalone or test-runner compilation. */
static char *
lsqa_getenv_dup(char const *name)
{
#if defined(_MSC_VER)
    char *value;
    size_t length;

    value = NULL;
    length = 0;
    if (_dupenv_s(&value, &length, name) != 0) {
        free(value);
        return NULL;
    }
    return value;
#else
    char const *value;
    size_t length;
    char *copy;

    value = getenv(name);
    if (value == NULL) {
        return NULL;
    }
    length = strlen(value);
    copy = malloc(length + 1);
    if (copy != NULL) {
        memcpy(copy, value, length + 1);
    }
    return copy;
#endif
}

#endif
