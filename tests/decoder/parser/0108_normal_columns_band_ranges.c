/* SPDX-License-Identifier: MIT */
/* A requested band range changes only its rows, including the final row. */
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/decoder-image.h"

int
test_decoder_0098(int argc, char **argv)
{
    sixel_allocator_t *allocator;
    image_buffer_t *image;
    SIXELSTATUS status;
    int depth;
    int keep;
    int y;
    int x;
    int channel;
    int expected;
    int ok;

    (void)argc;
    (void)argv;
    allocator = NULL;
    image = NULL;
    ok = 0;
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) { goto done; }
    image = calloc(1u, sizeof(*image));
    if (image == NULL) { goto done; }
    for (depth = 1; depth <= 4; depth *= 2) {
        for (keep = 0; keep < 2; ++keep) {
            status = image_buffer_init(image, 2, 7, 9, depth, keep, allocator);
            if (SIXEL_FAILED(status)) { goto done; }
            image->normal_columns = calloc(4u, sizeof(uint64_t));
            if (image->normal_columns == NULL) { goto done; }
            image->normal_columns[0] = UINT64_C(0x003f010101010101);
            image->normal_columns[2] = UINT64_C(0x003f010101010101);
            image->palette[1] = 0xff0000;
            image_buffer_finalize_normal_bands(image, 1, 2);
            for (y = 0; y < 7; ++y) {
                for (x = 0; x < 2; ++x) {
                    if (depth == 4) {
                        for (channel = 0; channel < 4; ++channel) {
                            expected = y == 6 && x == 0 &&
                                (channel == 0 || channel == 3) ? 255 : 0;
                            if (image->pixels.in_bytes[
                                    (y * 2 + x) * 4 + channel] != expected) {
                                goto done;
                            }
                        }
                    } else {
                        expected = y == 6 && x == 0 ? 1 : 9;
                        if ((depth == 1 ? image->pixels.in_bytes[y * 2 + x] :
                             image->pixels.in_shorts[y * 2 + x]) != expected) {
                            goto done;
                        }
                    }
                    if (keep && image->paint_mask[y * 2 + x] !=
                            (y == 6 && x == 0 ? 255 : 0)) { goto done; }
                }
            }
            free(image->normal_columns); free(image->pixels.p);
            free(image->paint_mask);
            memset(image, 0, sizeof(*image));
        }
    }
    ok = 1;
done:
    if (image != NULL) {
        free(image->normal_columns); free(image->pixels.p);
        free(image->paint_mask); free(image);
    }
    sixel_allocator_unref(allocator);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
