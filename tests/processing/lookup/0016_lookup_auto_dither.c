/* SPDX-License-Identifier: MIT
 * Policy: docs/encoder/palette-application/lookup-policy/README.md
 * AUTO keeps exact lookup while selecting bulk versus causal point mapping.
 */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include <6cells.h>
#include "src/compat_stub.h"
#include "src/lookup-policy.h"

/* Read a completed converter-owned file. GNV shell device names do not
 * guarantee that native C fopen reaches the shell's redirected stderr. */
static int
auto_dither_trace(char const *path, char const *method)
{
    FILE *file;
    char line[1024];
    int expect_batch;
    int backend;
    int strategy;
    int batch;
    int read_error;

    expect_batch = strcmp(method, "none") == 0;
    if (!expect_batch && strcmp(method, "fs") != 0 &&
            strcmp(method, "lso2") != 0) {
        return 0;
    }
    file = sixel_compat_fopen(path, "rb");
    if (file == NULL) {
        perror(path);
        return 0;
    }
    backend = strategy = batch = 0;
    while (fgets(line, sizeof(line), file) != NULL) {
        if (strstr(line, "\"event\":\"lookup/vptree.8bit\"") != NULL &&
                strstr(line, "\"role\":\"lookup\"") != NULL) {
            backend = 1;
            strategy |= strstr(line, expect_batch ? "\"worker\":\"batch\""
                                        : "\"worker\":\"point\"") != NULL;
        }
        if (strstr(line, "\"event\":\"none-batch\"") != NULL) {
            batch = 1;
        }
    }
    read_error = ferror(file);
    fclose(file);
    if (read_error || !backend || !strategy || batch != expect_batch) {
        fprintf(stderr, "%s timeline: backend=%d strategy=%d batch=%d "
                "expected_batch=%d read_error=%d\n",
                method, backend, strategy, batch, expect_batch, read_error);
        return 0;
    }
    return 1;
}

int
test_lookup_0016_auto_dither(int argc, char **argv)
{
    sixel_lookup_policy_select_request_t request;
    int methods[] = { SIXEL_DIFFUSE_NONE, SIXEL_DIFFUSE_FS,
                      SIXEL_DIFFUSE_LSO2, SIXEL_DIFFUSE_X_DITHER,
                      SIXEL_DIFFUSE_AUTO };
    int i;
    int f;
    int explicit_policy;
    char const *name;

    if (argc == 3) {
        return auto_dither_trace(argv[1], argv[2])
            ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    if (argc != 1) {
        return EXIT_FAILURE;
    }
    memset(&request, 0, sizeof(request));
    request.depth = 3;
    request.reqcolor = 256;
    request.optimize_lookup = 1;
    for (f = 0; f < 2; ++f) {
        request.pixelformat = f ? SIXEL_PIXELFORMAT_RGBFLOAT32
                               : SIXEL_PIXELFORMAT_RGB888;
        for (i = 0; i < 5; ++i) {
            request.method_for_diffuse = methods[i];
            if (sixel_lookup_policy_select_batch(methods[i]) != (i == 0)) {
                return EXIT_FAILURE;
            }
            for (explicit_policy = 0; explicit_policy < 2; ++explicit_policy) {
                request.lut_policy = explicit_policy ? SIXEL_LUT_POLICY_VPTREE
                                                    : SIXEL_LUT_POLICY_AUTO;
                name = sixel_lookup_policy_select_name_with_override(
                    &request, explicit_policy);
                if (strcmp(name, f ? "lookup/vptree.float32"
                                   : "lookup/vptree.8bit") != 0) {
                    return EXIT_FAILURE;
                }
            }
            request.lut_policy = SIXEL_LUT_POLICY_6BIT;
            name = sixel_lookup_policy_select_name_with_override(&request, 1);
            if (strcmp(name, f ? "lookup/6bit.float32"
                               : "lookup/6bit.8bit") != 0) {
                return EXIT_FAILURE;
            }
        }
    }
    return EXIT_SUCCESS;
}
