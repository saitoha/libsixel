/*
 * Shared helpers for exercising sixel_helper_write_image_file() with
 * pngsuite samples.
 *
 * Flow summary (successful write):
 * 1. Resolve the pngsuite file path relative to the source root.
 * 2. Load the image into a frame with the loader API.
 * 3. Call sixel_helper_write_image_file() using the expected pixelformat.
 * 4. Confirm a non-empty PNG file is written.
 */

#ifndef API_TEST_COMMON_H
#define API_TEST_COMMON_H

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sixel.h>

extern int mkstemp(char *template);

extern int sixel_compat_snprintf(char *buffer,
                                 size_t buffer_size,
                                 char const *format,
                                 ...);
extern FILE *sixel_compat_fopen(char const *filename, char const *mode);
extern char const *sixel_compat_getenv(char const *name);

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#if defined(__clang__)
# if __has_attribute(unused)
#  define API_TEST_UNUSED __attribute__((unused))
# else
#  define API_TEST_UNUSED
# endif
#elif defined(__GNUC__)
# define API_TEST_UNUSED __attribute__((unused))
#else
# define API_TEST_UNUSED
#endif

#define PNGSUITE_G1_PATH \
    "/images/pngsuite/basic/basn0g01.png"
#define PNGSUITE_G2_PATH \
    "/images/pngsuite/basic/basn0g02.png"
#define PNGSUITE_G4_PATH \
    "/images/pngsuite/basic/basn0g04.png"
#define PNGSUITE_G8_PATH \
    "/images/pngsuite/basic/basn0g08.png"
#define PNGSUITE_PAL1_PATH \
    "/images/pngsuite/basic/basn3p01.png"
#define PNGSUITE_PAL2_PATH \
    "/images/pngsuite/basic/basn3p02.png"
#define PNGSUITE_PAL4_PATH \
    "/images/pngsuite/basic/basn3p04.png"
#define PNGSUITE_PAL8_PATH \
    "/images/pngsuite/basic/basn3p08.png"
#define PNGSUITE_RGB_PATH \
    "/images/pngsuite/basic/basn2c08.png"
#define PNGSUITE_RGBA_PATH \
    "/images/pngsuite/basic/basn6a08.png"

#define SIXEL_TEST_SKIP 77

typedef struct api_write_context {
    sixel_frame_t *frame;
    int frame_count;
} api_write_context_t;

static SIXELSTATUS
capture_loaded_frame(sixel_frame_t *frame, void *data)
{
    api_write_context_t *context;

    context = (api_write_context_t *)data;
    context->frame_count += 1;
    if (context->frame == NULL) {
        context->frame = frame;
        sixel_frame_ref(frame);
    }

    return SIXEL_OK;
}

static API_TEST_UNUSED int
api_test_build_image_path(char const *source_root,
                          char const *relative,
                          char *buffer,
                          size_t capacity)
{
    int written;

    written = sixel_compat_snprintf(buffer,
                                    capacity,
                                    "%s%s",
                                    source_root,
                                    relative);
    if (written < 0 || (size_t)written >= capacity) {
        return 1;
    }

    return 0;
}

static API_TEST_UNUSED char const *
api_test_resolve_source_root(void)
{
    char const *source_root;

    source_root = sixel_compat_getenv("MESON_SOURCE_ROOT");
    if (source_root == NULL) {
        source_root = sixel_compat_getenv("abs_top_srcdir");
    }
    if (source_root == NULL) {
        source_root = sixel_compat_getenv("TOP_SRCDIR");
    }
    if (source_root == NULL) {
        source_root = ".";
    }

    return source_root;
}

static API_TEST_UNUSED int
load_pngsuite_frame(char const *label,
                    char const *relative_path,
                    sixel_allocator_t *allocator,
                    sixel_frame_t **out_frame)
{
    SIXELSTATUS status;
    api_write_context_t context;
    sixel_loader_t *loader;
    char const *source_root;
    char image_path[PATH_MAX];
    int require_static;
    int use_palette;
    int reqcolors;
    int result;

    loader = NULL;
    result = 1;
    context.frame = NULL;
    context.frame_count = 0;

    source_root = api_test_resolve_source_root();
    if (api_test_build_image_path(source_root,
                                  relative_path,
                                  image_path,
                                  sizeof(image_path)) != 0) {
        fprintf(stderr, "%s: failed to build pngsuite path\n", label);
        return 1;
    }

    status = sixel_loader_new(&loader, allocator);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: loader initialization failed\n", label);
        goto cleanup;
    }

    require_static = 1;
    use_palette = 1;
    reqcolors = 256;

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_REQUIRE_STATIC,
                                 &require_static);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: failed to set loader static option\n", label);
        goto cleanup;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_USE_PALETTE,
                                 &use_palette);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: failed to set loader palette option\n", label);
        goto cleanup;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_REQCOLORS,
                                 &reqcolors);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: failed to set loader color option\n", label);
        goto cleanup;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_CONTEXT,
                                 &context);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: failed to set loader context\n", label);
        goto cleanup;
    }

    status = sixel_loader_load_file(loader, image_path, capture_loaded_frame);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: loader failed with status %d\n",
                label,
                (int)status);
        goto cleanup;
    }

    if (context.frame == NULL || context.frame_count != 1) {
        fprintf(stderr, "%s: unexpected frame count %d\n",
                label,
                context.frame_count);
        goto cleanup;
    }

    *out_frame = context.frame;
    result = 0;

cleanup:
    if (result != 0 && context.frame != NULL) {
        sixel_frame_unref(context.frame);
    }
    if (loader != NULL) {
        sixel_loader_unref(loader);
    }

    return result;
}

static API_TEST_UNUSED int
ensure_non_empty_file(char const *label, char const *path)
{
    FILE *fp;
    int first_byte;

    fp = sixel_compat_fopen(path, "rb");
    if (fp == NULL) {
        fprintf(stderr, "%s: failed to open output file\n", label);
        return 1;
    }

    first_byte = fgetc(fp);
    fclose(fp);

    if (first_byte == EOF) {
        fprintf(stderr, "%s: output file is empty\n", label);
        return 1;
    }

    return 0;
}

static API_TEST_UNUSED int
make_temp_path(char *buffer, size_t capacity)
{
    char const *tmpdir;
    int written;
    int fd;

    tmpdir = sixel_compat_getenv("TMPDIR");
    if (tmpdir == NULL) {
        tmpdir = "/tmp";
    }

    written = sixel_compat_snprintf(buffer,
                                    capacity,
                                    "%s/sixel-api-XXXXXX",
                                    tmpdir);
    if (written < 0 || (size_t)written >= capacity) {
        return 1;
    }

    fd = mkstemp(buffer);
    if (fd < 0) {
        return 1;
    }
    (void)close(fd);
    if (unlink(buffer) != 0) {
        return 1;
    }

    return 0;
}

static API_TEST_UNUSED int
convert_g8_to_gray(unsigned char const *src,
                   int width,
                   int height,
                   int bits,
                   unsigned char **dst_out,
                   sixel_allocator_t *allocator)
{
    size_t count;
    unsigned char *dst;
    int max_value;
    size_t index;

    count = (size_t)width * (size_t)height;
    dst = sixel_allocator_malloc(allocator, count);
    if (dst == NULL) {
        return 1;
    }

    max_value = (1 << bits) - 1;
    for (index = 0; index < count; index++) {
        dst[index] = (unsigned char)
            ((src[index] * max_value + 127) / 255);
    }

    *dst_out = dst;
    return 0;
}

static API_TEST_UNUSED int
convert_rgb_to_rgba(unsigned char const *src,
                    int width,
                    int height,
                    unsigned char **dst_out,
                    sixel_allocator_t *allocator)
{
    size_t count;
    unsigned char *dst;
    size_t index;
    size_t src_offset;
    size_t dst_offset;

    count = (size_t)width * (size_t)height;
    dst = sixel_allocator_malloc(allocator, count * 4);
    if (dst == NULL) {
        return 1;
    }

    for (index = 0; index < count; index++) {
        src_offset = index * 3;
        dst_offset = index * 4;
        dst[dst_offset] = src[src_offset];
        dst[dst_offset + 1] = src[src_offset + 1];
        dst[dst_offset + 2] = src[src_offset + 2];
        dst[dst_offset + 3] = 0xff;
    }

    *dst_out = dst;
    return 0;
}

static API_TEST_UNUSED int
run_write_image_case_impl(char const *label,
                          char const *relative_path,
                          int expected_pixelformat,
                          SIXELSTATUS expected_status,
                          int expect_output)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_frame_t *frame;
    unsigned char *pixels;
    unsigned char *palette;
    int pixelformat;
    unsigned char *work_pixels;
    unsigned char *owned_pixels;
    int work_pixelformat;
    int width;
    int height;
    int gray_bits;
    char output_path[PATH_MAX];
    int exit_status;

    allocator = NULL;
    frame = NULL;
    pixels = NULL;
    palette = NULL;
    pixelformat = 0;
    work_pixels = NULL;
    owned_pixels = NULL;
    work_pixelformat = 0;
    width = 0;
    height = 0;
    gray_bits = 0;
    output_path[0] = '\0';
    exit_status = 1;

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: allocator initialization failed\n", label);
        return EXIT_FAILURE;
    }

    if (load_pngsuite_frame(label,
                            relative_path,
                            allocator,
                            &frame) != 0) {
        goto cleanup;
    }

    pixelformat = sixel_frame_get_pixelformat(frame);

    pixels = sixel_frame_get_pixels(frame);
    if (pixels == NULL) {
        fprintf(stderr, "%s: missing pixel data\n", label);
        goto cleanup;
    }

    if ((pixelformat & SIXEL_FORMATTYPE_PALETTE) != 0) {
        palette = sixel_frame_get_palette(frame);
        if (palette == NULL) {
            fprintf(stderr, "%s: missing palette data\n", label);
            goto cleanup;
        }
    }

    width = sixel_frame_get_width(frame);
    height = sixel_frame_get_height(frame);

    work_pixels = pixels;
    work_pixelformat = pixelformat;

    if (pixelformat != expected_pixelformat) {
        if (expected_pixelformat == SIXEL_PIXELFORMAT_G1
            || expected_pixelformat == SIXEL_PIXELFORMAT_G2
            || expected_pixelformat == SIXEL_PIXELFORMAT_G4) {
            if (pixelformat != SIXEL_PIXELFORMAT_G8) {
                fprintf(stderr,
                        "%s: unexpected pixelformat %d\n",
                        label,
                        pixelformat);
                goto cleanup;
            }
            if (expected_pixelformat == SIXEL_PIXELFORMAT_G1) {
                gray_bits = 1;
            } else if (expected_pixelformat == SIXEL_PIXELFORMAT_G2) {
                gray_bits = 2;
            } else {
                gray_bits = 4;
            }
            /*
             * pngsuite grayscale samples are loaded as G8, so down-convert
             * them to the requested sub-byte layout to exercise that
             * pixelformat path without rewriting the loader output.
             */
            if (convert_g8_to_gray(pixels,
                                   width,
                                   height,
                                   gray_bits,
                                   &owned_pixels,
                                   allocator) != 0) {
                fprintf(stderr,
                        "%s: failed to convert grayscale buffer\n",
                        label);
                goto cleanup;
            }
            work_pixels = owned_pixels;
            work_pixelformat = expected_pixelformat;
        } else if (expected_pixelformat == SIXEL_PIXELFORMAT_RGBA8888
                   && pixelformat == SIXEL_PIXELFORMAT_RGB888) {
            if (convert_rgb_to_rgba(pixels,
                                    width,
                                    height,
                                    &owned_pixels,
                                    allocator) != 0) {
                fprintf(stderr,
                        "%s: failed to expand RGBA buffer\n",
                        label);
                goto cleanup;
            }
            work_pixels = owned_pixels;
            work_pixelformat = expected_pixelformat;
        } else {
            fprintf(stderr,
                    "%s: unexpected pixelformat %d\n",
                    label,
                    pixelformat);
            goto cleanup;
        }
    }

    if (make_temp_path(output_path, sizeof(output_path)) != 0) {
        fprintf(stderr, "%s: failed to allocate temp path\n", label);
        goto cleanup;
    }

    status = sixel_helper_write_image_file(work_pixels,
                                           width,
                                           height,
                                           palette,
                                           work_pixelformat,
                                           output_path,
                                           SIXEL_FORMAT_PNG,
                                           allocator);
    if (expected_status == SIXEL_OK) {
        if (SIXEL_FAILED(status)) {
            fprintf(stderr,
                    "%s: write_image_file failed (%d)\n",
                    label,
                    (int)status);
            goto cleanup;
        }
        if (expect_output != 0
            && ensure_non_empty_file(label, output_path) != 0) {
            goto cleanup;
        }
        exit_status = 0;
    } else if (status == expected_status) {
        exit_status = 0;
    } else {
        fprintf(stderr,
                "%s: unexpected status %d (expected %d)\n",
                label,
                (int)status,
                (int)expected_status);
        goto cleanup;
    }

cleanup:
    if (output_path[0] != '\0') {
        unlink(output_path);
    }
    if (owned_pixels != NULL) {
        sixel_allocator_free(allocator, owned_pixels);
    }
    if (frame != NULL) {
        sixel_frame_unref(frame);
    }
    if (allocator != NULL) {
        sixel_allocator_unref(allocator);
    }

    return exit_status == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

static API_TEST_UNUSED int
run_write_image_case(char const *label,
                     char const *relative_path,
                     int expected_pixelformat)
{
    return run_write_image_case_impl(label,
                                     relative_path,
                                     expected_pixelformat,
                                     SIXEL_OK,
                                     1);
}

static API_TEST_UNUSED int
run_write_image_case_expect_failure(char const *label,
                                    char const *relative_path,
                                    int expected_pixelformat,
                                    SIXELSTATUS expected_status)
{
    return run_write_image_case_impl(label,
                                     relative_path,
                                     expected_pixelformat,
                                     expected_status,
                                     0);
}

#endif

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
