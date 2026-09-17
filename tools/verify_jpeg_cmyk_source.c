/* SPDX-License-Identifier: MIT */
/* Emit decoded CMYK bytes without RGB conversion or any CMM.
 * Build: cc verify_jpeg_cmyk_source.c -ljpeg -o verify-jpeg-cmyk-source
 * The output verifies RAW in generate_jpeg_cmyk_tests.py. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jpeglib.h>

int
main(int argc, char **argv)
{
    struct jpeg_decompress_struct decoder;
    struct jpeg_error_mgr error;
    FILE *file;
    JSAMPARRAY row;
#if defined(MAXJ12SAMPLE)
    J12SAMPARRAY row12;
#endif
#if defined(MAXJ16SAMPLE)
    J16SAMPARRAY row16;
#endif
    unsigned int sample;
    unsigned int column;
    unsigned int stride;
    int extended;
    int gray16;
    int ok;

    gray16 = argc == 3 && strcmp(argv[1], "--gray16") == 0;
    extended = gray16 ||
        (argc == 3 && strcmp(argv[1], "--full-precision") == 0);
    if (argc != 2 && !extended) { return EXIT_FAILURE; }
    file = fopen(argv[extended ? 2 : 1], "rb");
    if (file == NULL) { return EXIT_FAILURE; }
    decoder.err = jpeg_std_error(&error);
    jpeg_create_decompress(&decoder);
    jpeg_stdio_src(&decoder, file);
    jpeg_read_header(&decoder, TRUE);
    decoder.out_color_space = gray16 ? JCS_GRAYSCALE : JCS_CMYK;
    decoder.dct_method = JDCT_ISLOW;
    jpeg_start_decompress(&decoder);
    if (extended) {
        /* Original device sample units, before scaling, inversion or CMS.
         * The text header is followed by big-endian uint16 components. */
        ok = decoder.output_components == (gray16 ? 1 : 4) &&
             (!gray16 || decoder.data_precision == 16);
        stride = decoder.output_width * (unsigned int)decoder.output_components;
        printf("%s %u %u %d %d %d %u\n", gray16 ? "GRAY" : "CMYK",
            decoder.output_width, decoder.output_height, decoder.data_precision,
            decoder.jpeg_color_space, decoder.saw_Adobe_marker,
            decoder.Adobe_transform);
        row = NULL;
#if defined(MAXJ12SAMPLE)
        row12 = NULL;
        if (decoder.data_precision == 12) {
            row12 = (J12SAMPARRAY)(*decoder.mem->alloc_sarray)(
                (j_common_ptr)&decoder,
                JPOOL_IMAGE, stride, 1u);
        }
#endif
#if defined(MAXJ16SAMPLE)
        row16 = NULL;
        if (decoder.data_precision == 16) {
            row16 = (J16SAMPARRAY)(*decoder.mem->alloc_sarray)(
                (j_common_ptr)&decoder,
                JPOOL_IMAGE, stride, 1u);
        }
#endif
        if (decoder.data_precision == 8) {
            row = (*decoder.mem->alloc_sarray)((j_common_ptr)&decoder,
                JPOOL_IMAGE, stride, 1u);
        }
        while (ok && decoder.output_scanline < decoder.output_height) {
            if (decoder.data_precision == 8) {
                ok = jpeg_read_scanlines(&decoder, row, 1u) == 1u;
#if defined(MAXJ12SAMPLE)
            } else if (decoder.data_precision == 12) {
                ok = jpeg12_read_scanlines(&decoder, row12, 1u) == 1u;
#endif
#if defined(MAXJ16SAMPLE)
            } else if (decoder.data_precision == 16) {
                ok = jpeg16_read_scanlines(&decoder, row16, 1u) == 1u;
#endif
            } else {
                ok = 0;
            }
            for (column = 0u; ok && column < stride; ++column) {
                sample = 0u;
                if (row != NULL) { sample = row[0][column]; }
#if defined(MAXJ12SAMPLE)
                if (row12 != NULL) { sample = row12[0][column]; }
#endif
#if defined(MAXJ16SAMPLE)
                if (row16 != NULL) { sample = row16[0][column]; }
#endif
                ok &= putchar((int)(sample >> 8)) != EOF;
                ok &= putchar((int)(sample & 255u)) != EOF;
            }
        }
        if (ok) { jpeg_finish_decompress(&decoder); }
    } else {
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
    }
    jpeg_destroy_decompress(&decoder);
    fclose(file);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
