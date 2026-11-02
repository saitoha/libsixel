/* SPDX-License-Identifier: MIT */
/* Shared compile/link probe for the external PNG reader and writer.
 * Never execute this program: the calls check the API, not PNG validity.
 * Keep input-dependent metadata rejection in the independent getter tests.
 */
#include <stddef.h>
#include <setjmp.h>
#include <png.h>

#if !defined(PNG_READ_SUPPORTED) || !defined(PNG_WRITE_SUPPORTED)
# error libpng must support both reading and writing
#endif
#if !defined(PNG_FLOATING_POINT_SUPPORTED) || !defined(PNG_SETJMP_SUPPORTED)
# error libpng must expose floating metadata and setjmp error handling
#endif

int
main(int argc, char **argv)
{
    png_structp png;
    png_infop info;
    png_colorp palette;
    png_color_16p background;
    png_bytep bytes;
    png_bytepp rows;
    png_charp name;
    png_uint_32 length;
    double values[8];
    int count;
    int compression;
    int result;

    /* Check declarations before calls, including with compilers that
     * otherwise allow implicit function declarations. The calls below also
     * require the matching symbols from the selected library. */
    (void)png_create_info_struct;
    (void)png_create_read_struct;
    (void)png_create_write_struct;
    (void)png_destroy_read_struct;
    (void)png_destroy_write_struct;
    (void)png_error;
    (void)png_get_PLTE;
    (void)png_get_bKGD;
    (void)png_get_bit_depth;
    (void)png_get_cHRM;
    (void)png_get_channels;
    (void)png_get_color_type;
    (void)png_get_gAMA;
    (void)png_get_iCCP;
    (void)png_get_image_height;
    (void)png_get_image_width;
    (void)png_get_io_ptr;
    (void)png_get_rowbytes;
    (void)png_get_tRNS;
    (void)png_get_valid;
    (void)png_longjmp;
    (void)png_read_end;
    (void)png_read_image;
    (void)png_read_info;
    (void)png_read_update_info;
    (void)png_set_IHDR;
    (void)png_set_PLTE;
    (void)png_set_add_alpha;
    (void)png_set_gray_to_rgb;
    (void)png_set_iCCP;
    (void)png_set_interlace_handling;
    (void)png_set_packing;
    (void)png_set_palette_to_rgb;
    (void)png_set_read_fn;
    (void)png_set_sRGB;
    (void)png_set_strip_alpha;
    (void)png_set_tRNS_to_alpha;
    (void)png_set_write_fn;
    (void)png_write_end;
    (void)png_write_image;
    (void)png_write_info;

    bytes = (png_bytep)argv[0];
    rows = &bytes;
    palette = NULL;
    background = NULL;
    name = NULL;
    length = 0;
    count = 0;
    compression = 0;
    png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    info = png_create_info_struct(png);
    result = setjmp(png_jmpbuf(png));
    png_set_read_fn(png, NULL, NULL);
    png_read_info(png, info);
    result += png_get_gAMA(png, info, &values[0]) != 0;
    result += png_get_cHRM(png, info, &values[0], &values[1], &values[2],
                          &values[3], &values[4], &values[5], &values[6],
                          &values[7]) != 0;
    result += png_get_iCCP(png, info, &name, &compression, &bytes, &length)
              != 0;
    result += png_get_PLTE(png, info, &palette, &count) != 0;
    result += png_get_bKGD(png, info, &background) != 0;
    result += png_get_tRNS(png, info, &bytes, &count, &background) != 0;
    result += png_get_valid(png, info, PNG_INFO_tRNS) != 0;
    result += png_get_image_width(png, info) != 0;
    result += png_get_image_height(png, info) != 0;
    result += png_get_bit_depth(png, info);
    result += png_get_channels(png, info);
    result += png_get_color_type(png, info);
    result += png_get_rowbytes(png, info) != 0;
    result += png_get_io_ptr(png) != NULL;
    result += png_set_interlace_handling(png);
    png_set_add_alpha(png, 255, PNG_FILLER_AFTER);
    png_set_gray_to_rgb(png);
    png_set_packing(png);
    png_set_palette_to_rgb(png);
    png_set_strip_alpha(png);
    png_set_tRNS_to_alpha(png);
#if HAVE_DECL_PNG_SET_EXPAND_GRAY_1_2_4_TO_8
    (void)png_set_expand_gray_1_2_4_to_8;
    png_set_expand_gray_1_2_4_to_8(png);
#elif HAVE_DECL_PNG_SET_GRAY_1_2_4_TO_8
    (void)png_set_gray_1_2_4_to_8;
    png_set_gray_1_2_4_to_8(png);
#else
# error libpng must expose low-depth gray expansion
#endif
#if defined(PNG_SET_OPTION_SUPPORTED) && defined(PNG_SKIP_sRGB_CHECK_PROFILE)
    (void)png_set_option;
    png_set_option(png, PNG_SKIP_sRGB_CHECK_PROFILE, PNG_OPTION_ON);
#endif
    png_read_update_info(png, info);
    png_read_image(png, rows);
    png_read_end(png, info);
    if (argc == 2) {
        png_error(png, "compile/link probe");
    }
    if (argc == 3) {
        png_longjmp(png, 1);
    }
    png_destroy_read_struct(&png, &info, NULL);
    png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    info = png_create_info_struct(png);
    png_set_write_fn(png, NULL, NULL, NULL);
    png_set_IHDR(png, info, 1, 1, 8, PNG_COLOR_TYPE_RGB,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    png_set_PLTE(png, info, palette, count);
    png_set_iCCP(png, info, "RGB", 0, bytes, length);
    png_set_sRGB(png, info, PNG_sRGB_INTENT_PERCEPTUAL);
    png_write_info(png, info);
    png_write_image(png, rows);
    png_write_end(png, info);
    png_destroy_write_struct(&png, &info);
    return result;
}
