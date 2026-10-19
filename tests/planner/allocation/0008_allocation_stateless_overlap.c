/* SPDX-License-Identifier: MIT */
/* Compare decoded pixels, allowing redundant SIXEL register selections. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/compat_stub.h"

int
test_allocation_stateless_overlap(int argc, char **argv)
{
    sixel_allocator_t *allocator;
    sixel_decode_options_t options;
    sixel_decode_result_t decoded[3];
    FILE *file;
    unsigned char *data;
    long size;
    int side;
    int row;
    int ok;

    allocator = NULL;
    file = NULL;
    data = NULL;
    ok = 0;
    memset(&options, 0, sizeof(options));
    memset(decoded, 0, sizeof(decoded));
    options.preferred_pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
    if (argc != 4 || SIXEL_FAILED(sixel_allocator_new(&allocator,
            NULL, NULL, NULL, NULL))) {
        goto end;
    }
    /* Files keep three complete streams out of Windows command-line length
     * limits. Every byte written by the shell is consumed by this check. */
    for (side = 0; side < 3; ++side) {
        file = sixel_compat_fopen(argv[side + 1], "rb");
        if (file == NULL || fseek(file, 0L, SEEK_END) != 0) {
            goto end;
        }
        size = ftell(file);
        if (size <= 0 || size > 1024 * 1024 ||
                fseek(file, 0L, SEEK_SET) != 0) {
            goto end;
        }
        data = (unsigned char *)malloc((size_t)size);
        if (data == NULL || fread(data, 1u, (size_t)size, file)
                != (size_t)size) {
            goto end;
        }
        (void)fclose(file);
        file = NULL;
        if (SIXEL_FAILED(sixel_decode_pixels(data, (size_t)size,
                &options, decoded + side, allocator)) ||
                decoded[side].width != 64 || decoded[side].height != 64 ||
                decoded[side].pixelformat != SIXEL_PIXELFORMAT_RGBA8888 ||
                decoded[side].pixels == NULL || decoded[side].stride < 256) {
            goto end;
        }
        free(data);
        data = NULL;
    }
    for (side = 1; side < 3; ++side) {
        for (row = 0; row < 64; ++row) {
            if (memcmp(decoded[0].pixels + row * decoded[0].stride,
                    decoded[side].pixels + row * decoded[side].stride,
                    256u) != 0) {
                fprintf(stderr, "Stateless output %d differs at row %d\n",
                    side, row);
                goto end;
            }
        }
    }
    ok = 1;
end:
    if (file != NULL) {
        (void)fclose(file);
    }
    free(data);
    if (allocator != NULL) {
        for (side = 0; side < 3; ++side) {
            sixel_allocator_free(allocator, decoded[side].pixels);
        }
        sixel_allocator_unref(allocator);
    }
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
