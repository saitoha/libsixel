/*
 * SPDX-License-Identifier: MIT
 *
 * Exercise the runtime-specific path adapter contract through both copies.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "converters/path.h"
#include "src/path.h"

typedef size_t (*path_size_fn)(char const *path);
typedef char const *(*path_convert_fn)(char const *path,
                                      char *buffer,
                                      size_t buffer_size);

static int
copy_result(char const *name,
            char const *result,
            char *snapshot,
            size_t snapshot_size)
{
    size_t length;

    if (result == NULL) {
        fprintf(stderr, "%s returned a null result\n", name);
        return 0;
    }
    length = strlen(result);
    if (length + 1u > snapshot_size) {
        fprintf(stderr, "%s result exceeds the snapshot buffer\n", name);
        return 0;
    }
    memcpy(snapshot, result, length + 1u);
    return 1;
}

static int
check_adapter(char const *name,
              path_size_fn size_fn,
              path_convert_fn convert_fn,
              char *snapshot,
              size_t snapshot_size)
{
    static char const *protocol_paths[] = {
        "",
        "-",
        "clipboard:"
    };
    char buffer[512];
    char const *path;
    char const *result;
    size_t index;
    size_t needed;

    if (size_fn(NULL) != 0u || convert_fn(NULL, NULL, 0u) != NULL) {
        fprintf(stderr, "%s changed null-input handling\n", name);
        return 0;
    }

    for (index = 0u;
         index < sizeof(protocol_paths) / sizeof(protocol_paths[0]);
         index++) {
        path = protocol_paths[index];
        if (size_fn(path) != 0u || convert_fn(path, NULL, 0u) != path) {
            fprintf(stderr, "%s rewrote protocol value: %s\n", name, path);
            return 0;
        }
    }

#if (defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MSYS__)) \
    || defined(__EMSCRIPTEN__) || defined(__COSMOPOLITAN__)
    path = "/c/platform/file";
#elif defined(__CYGWIN__) || defined(__MSYS__)
    path = "C:/platform/file";
#else
    path = "relative/platform/file";
#endif

    needed = size_fn(path);
#if defined(__CYGWIN__) || defined(__MSYS__)
    if (needed == 0u) {
        fprintf(stderr, "%s bypassed runtime-owned path conversion\n", name);
        return 0;
    }
#endif
    if (needed == 0u) {
        result = convert_fn(path, NULL, 0u);
        if (result != path) {
            fprintf(stderr, "%s did not preserve an unchanged path\n", name);
            return 0;
        }
        return copy_result(name, result, snapshot, snapshot_size);
    }

    if (needed > sizeof(buffer)) {
        fprintf(stderr, "%s requested an unreasonable buffer\n", name);
        return 0;
    }
    if (convert_fn(path, NULL, needed) != NULL
        || convert_fn(path, buffer, needed - 1u) != NULL) {
        fprintf(stderr, "%s accepted insufficient storage\n", name);
        return 0;
    }
    result = convert_fn(path, buffer, needed);
    if (result != buffer || strlen(result) + 1u != needed) {
        fprintf(stderr, "%s broke the two-phase ownership contract\n", name);
        return 0;
    }
#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MSYS__)
    if (strcmp(result, "c:/platform/file") != 0) {
        fprintf(stderr, "%s produced an unexpected native path: %s\n",
                name, result);
        return 0;
    }
#endif
    return copy_result(name, result, snapshot, snapshot_size);
}

int
test_platform_path_0001_path_to_libc_runtime(int argc, char **argv)
{
    char library_result[512];
    char converter_result[512];

    (void)argc;
    (void)argv;

    if (!check_adapter("library path adapter",
                       sixel_path_to_libc_buffer_size,
                       sixel_path_to_libc,
                       library_result,
                       sizeof(library_result))) {
        return EXIT_FAILURE;
    }
    if (!check_adapter("converter path adapter",
                       img2sixel_path_to_libc_buffer_size,
                       img2sixel_path_to_libc,
                       converter_result,
                       sizeof(converter_result))) {
        return EXIT_FAILURE;
    }
    if (strcmp(library_result, converter_result) != 0) {
        fprintf(stderr, "library and converter path adapters diverged\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
