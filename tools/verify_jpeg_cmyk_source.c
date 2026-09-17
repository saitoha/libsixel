/* SPDX-License-Identifier: MIT */
/* Emit decoded CMYK bytes without RGB conversion or any CMM.
 * Build: cc verify_jpeg_cmyk_source.c -ljpeg -o verify-jpeg-cmyk-source
 * The output verifies RAW in generate_jpeg_cmyk_tests.py. */
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
    int ok;

    if (argc != 2) { return EXIT_FAILURE; }
    file = fopen(argv[1], "rb");
    if (file == NULL) { return EXIT_FAILURE; }
    decoder.err = jpeg_std_error(&error);
    jpeg_create_decompress(&decoder);
    jpeg_stdio_src(&decoder, file);
    jpeg_read_header(&decoder, TRUE);
    decoder.out_color_space = JCS_CMYK;
    decoder.dct_method = JDCT_ISLOW;
    jpeg_start_decompress(&decoder);
    ok = decoder.output_width == 32u && decoder.output_height == 32u &&
         decoder.output_components == 4 && decoder.data_precision == 8;
    if (ok) {
        row = (*decoder.mem->alloc_sarray)((j_common_ptr)&decoder,
            JPOOL_IMAGE, 128u, 1u);
        while (decoder.output_scanline < decoder.output_height) {
            ok &= jpeg_read_scanlines(&decoder, row, 1u) == 1u;
            ok &= fwrite(row[0], 1u, 128u, stdout) == 128u;
        }
        jpeg_finish_decompress(&decoder);
    }
    jpeg_destroy_decompress(&decoder);
    fclose(file);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
