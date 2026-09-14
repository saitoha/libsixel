/* SPDX-License-Identifier: MIT */
/* Independently inspect the fixed JPEG oracle before any RGB or ICC CMM.
 * Build manually with libjpeg: cc verify_jpeg_color_source.c -ljpeg.
 * This tool is not part of the normal libsixel build or TAP runtime. */
#include <stdio.h>
#include <stdlib.h>
#include <jpeglib.h>

int
main(int argc, char **argv)
{
    struct jpeg_decompress_struct decoder;
    struct jpeg_error_mgr error;
    FILE *file;
    JSAMPARRAY row;
    unsigned int y;
    unsigned int x;

    if (argc != 2) {
        return EXIT_FAILURE;
    }
    file = fopen(argv[1], "rb");
    if (file == NULL) {
        return EXIT_FAILURE;
    }
    decoder.err = jpeg_std_error(&error);
    jpeg_create_decompress(&decoder);
    jpeg_stdio_src(&decoder, file);
    jpeg_read_header(&decoder, TRUE);
    decoder.out_color_space = JCS_YCbCr;
    decoder.dct_method = JDCT_ISLOW;
    decoder.do_fancy_upsampling = TRUE;
    jpeg_start_decompress(&decoder);
    if (decoder.output_width != 64u || decoder.output_height != 64u ||
        decoder.output_components != 3) {
        jpeg_destroy_decompress(&decoder);
        fclose(file);
        return EXIT_FAILURE;
    }
#if defined(LIBJPEG_TURBO_VERSION_NUMBER)
    printf("libjpeg-turbo version %d\n", LIBJPEG_TURBO_VERSION_NUMBER);
#endif
    row = (*decoder.mem->alloc_sarray)((j_common_ptr)&decoder, JPOOL_IMAGE,
        decoder.output_width * 3u, 1u);
    while (decoder.output_scanline < decoder.output_height) {
        y = decoder.output_scanline;
        jpeg_read_scanlines(&decoder, row, 1u);
        if (y == 0u || y == 32u || y == 63u) {
            x = y == 63u ? 63u : 0u;
            printf("%u,%u YCbCr %u %u %u\n", x, y,
                row[0][x * 3u], row[0][x * 3u + 1u], row[0][x * 3u + 2u]);
        }
    }
    jpeg_finish_decompress(&decoder);
    jpeg_destroy_decompress(&decoder);
    fclose(file);
    return EXIT_SUCCESS;
}
