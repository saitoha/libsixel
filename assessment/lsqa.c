/*
 * lsqa.c - Thin CLI wrapper around the libsixel assessment engine.
 *
 *  Pipeline overview:
 *
 *        +---------------+    +------------------+    +-----------------+
 *        |  Frame input  | -> |  Assessment API  | -> |  JSON emission  |
 *        +---------------+    +------------------+    +-----------------+
 *             |                         |                        |
 *             |                         |                        +--> stdout
 *             |                         +--> sixel_assessment_t   +--> files
 *             +--> libsixel loader
 */

/*
 * Guard the feature test macros so unity builds do not complain when the
 * build system already defines the same POSIX surface area globally.  The
 * macros remain visible when missing so mkstemp() and POSIX strerror_r()
 * stay declared on platforms that expose it through feature macros.
 */
#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#if !defined(_XOPEN_SOURCE)
#define _XOPEN_SOURCE 700
#endif

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#if HAVE_STRINGS_H
# include <strings.h>
#endif

#if HAVE_MATH_H
# include <stddef.h>
#endif
#if HAVE_CTYPE_H
# include <ctype.h>
#endif
#if HAVE_ERRNO_H
# include <errno.h>
#endif
#if HAVE_LIMITS_H
# include <limits.h>
#endif
#if HAVE_STDINT_H
# include <stdint.h>
#endif
#if HAVE_MATH_H
# include <math.h>
#endif
#if HAVE_GETOPT_H
# include <getopt.h>
#endif

#include <sixel.h>

#include "assessment.h"
#include "scielab.h"
#include "video.h"
#include "color.h"
#include "sixel_decode_pixels.h"
#include "chunk-factory.h"
#include "chunk-view.h"
#include "getopt_stub.h"
#include "cli.h"
#include "decoder.h"
#include "loader.h"
#include "options.h"
#include "tty.h"

#if defined(_WIN32)
# if !defined(UNICODE)
#  define UNICODE
# endif
# if !defined(_UNICODE)
#  define _UNICODE
# endif
# if !defined(WIN32_LEAN_AND_MEAN)
#  define WIN32_LEAN_AND_MEAN
# endif
# include <windows.h>
# if HAVE_IO_H
#  include <io.h>
# endif
#elif HAVE_UNISTD_H
# include <unistd.h>
#endif

#if !defined(PATH_MAX)
#define PATH_MAX 4096
#endif

#if defined(_WIN32)
#define SIXEL_PATH_SEP '\\'
#else
#define SIXEL_PATH_SEP '/'
#endif

/*
 * Exit codes distinguish usage errors, runtime failures, and quality
 * regressions so automated callers can react deterministically.
 */
enum {
    LSQA_EXIT_SUCCESS = 0,
    LSQA_EXIT_BAD_ARGS = 2,
    LSQA_EXIT_LOAD_FAILED = 3,
    LSQA_EXIT_RUNTIME_FAILED = 4,
    LSQA_EXIT_BASELINE_FAILED = 5
};

/*
 * Local safety shims avoid MSVC secure CRT warnings without pulling in the
 * libsixel-wide compat layer. Each helper mirrors a single libc routine and
 * falls back to a bounded copy where secure variants are unavailable.
 */
static char const *
lsqa_strerror(int errnum, char *buffer, size_t size)
{
#if defined(_MSC_VER)
    errno_t rc;
#elif defined(LIBSIXEL_OPENVMS)
    char const *message;
    size_t length;
#endif

#if defined(_MSC_VER)
    if (buffer != NULL && size > 0) {
        rc = strerror_s(buffer, size, errnum);
        if (rc == 0) {
            return buffer;
        }
    }
    return "unknown error";
#elif defined(LIBSIXEL_OPENVMS)
    if (buffer != NULL && size > 0) {
        /*
         * OpenVMS CRTL can provide the strerror_r() symbol in GNV builds
         * without exposing a public prototype.  Use strerror() here to avoid
         * relying on an undeclared interface in this standalone CLI shim.
         */
        message = strerror(errnum);
        if (message != NULL) {
            length = strlen(message);
            if (length >= size) {
                length = size - 1u;
            }
            memcpy(buffer, message, length);
            buffer[length] = '\0';
            return buffer;
        }
    }
    return "unknown error";
#elif defined(HAVE_STRERROR_R)
    if (buffer != NULL && size > 0) {
        if (strerror_r(errnum, buffer, size) == 0) {
            return buffer;
        }
    }
    return "unknown error";
#else
    (void)buffer;
    (void)size;
    return strerror(errnum);
#endif
}

static char *
lsqa_getenv_dup(char const *name)
{
#if defined(_MSC_VER) && defined(HAVE__DUPENV_S)
    char *value;
    size_t len;
    errno_t rc;

    value = NULL;
    len = 0u;
    rc = _dupenv_s(&value, &len, name);
    if (rc != 0) {
        if (value != NULL) {
            free(value);
        }
        return NULL;
    }
    return value;
#else
    char const *value;
    size_t len;
    char *copy;

    value = getenv(name);
    if (value == NULL) {
        return NULL;
    }
    len = strlen(value);
    copy = (char *)malloc(len + 1u);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, value, len + 1u);
    return copy;
#endif
}

typedef struct LoaderCapture {
    sixel_frame_t *frame;
} LoaderCapture;

static SIXELSTATUS
capture_first_frame(sixel_frame_t *frame, void *context)
{
    LoaderCapture *capture;

    capture = (LoaderCapture *)context;
    if (capture->frame == NULL) {
        sixel_frame_ref(frame);
        capture->frame = frame;
    }
    return SIXEL_OK;
}

static int
load_frame(char const *path, sixel_chunk_t *chunk,
           sixel_allocator_t *allocator,
           char const *loader_order,
           unsigned char *bgcolor,
           sixel_frame_t **out_frame)
{
    LoaderCapture capture;
    SIXELSTATUS status;
    sixel_loader_t *loader;
    int fstatic;
    int fuse_palette;
    int reqcolors;
    int loop_override;
    int finsecure;

    capture.frame = NULL;
    loader = NULL;
    fstatic = 1;
    fuse_palette = 0;
    reqcolors = SIXEL_PALETTE_MAX;
    loop_override = SIXEL_LOOP_DISABLE;
    finsecure = 0;

    status = sixel_loader_new(&loader, allocator);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_REQUIRE_STATIC,
                                 &fstatic);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_USE_PALETTE,
                                 &fuse_palette);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_REQCOLORS,
                                 &reqcolors);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_LOOP_CONTROL,
                                 &loop_override);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_INSECURE,
                                 &finsecure);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    /* Mirror img2sixel -B semantics in loader-side alpha composition. */
    if (bgcolor != NULL) {
        status = sixel_loader_setopt(loader,
                                     SIXEL_LOADER_OPTION_BGCOLOR,
                                     bgcolor);
        if (SIXEL_FAILED(status)) {
            goto error;
        }
    }

    if (loader_order != NULL && loader_order[0] != '\0') {
        status = sixel_loader_setopt(loader,
                                     SIXEL_LOADER_OPTION_LOADER_ORDER,
                                     loader_order);
        if (SIXEL_FAILED(status)) {
            goto error;
        }
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_CONTEXT,
                                 &capture);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = chunk == NULL
        ? sixel_loader_load_file(loader, path, capture_first_frame)
        : sixel_loader_load_chunk(loader, chunk, capture_first_frame);
    if (SIXEL_FAILED(status)) {
        goto error;
    }
    if (capture.frame == NULL) {
        sixel_helper_set_additional_message(
            "loader did not produce any frame.");
        status = SIXEL_RUNTIME_ERROR;
        goto error;
    }

    sixel_loader_unref(loader);
    *out_frame = capture.frame;
    return 0;

error:
    if (SIXEL_FAILED(status)) {
        char const *detail;

        detail = sixel_helper_get_additional_message();
        if (detail != NULL && detail[0] != '\0') {
            fprintf(stderr,
                    "libsixel loader failed for %s: %s (%s)\n",
                    path,
                    sixel_helper_format_error(status),
                    detail);
        } else {
            fprintf(stderr,
                    "libsixel loader failed for %s: %s\n",
                    path,
                    sixel_helper_format_error(status));
        }
    }
    sixel_loader_unref(loader);
    if (capture.frame != NULL) {
        sixel_frame_unref(capture.frame);
    }
    return -1;
}

/* Decode original sRGB samples and compose holes before frame conversion.
 * Reuse the stateful CPU reconstruction path: it preserves masks and already
 * bypasses indexed reconstruction when palette registers are redefined. */
static SIXELSTATUS
load_direct_chunk(sixel_chunk_t *chunk, sixel_allocator_t *allocator,
                   unsigned char const *bg, int method, int similarity,
                   int edge, int threshold, sixel_frame_t **out_frame)
{
    sixel_decoder_t *decoder;
    sixel_decode_options_t options;
    sixel_decode_result_t result;
    sixel_frame_t *frame;
    SIXELSTATUS status;

    decoder = NULL;
    frame = NULL;
    memset(&options, 0, sizeof(options));
    memset(&result, 0, sizeof(result));
    options.preferred_pixelformat = SIXEL_PIXELFORMAT_RGB888;
    if (bg != NULL) {
        memcpy(options.bgcolor, bg, 3);
    }
    status = sixel_decoder_new(&decoder, allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    /* lsqa has no GPU policy option. Keep reconstruction on its existing
     * CPU algorithms, independently of converter environment defaults. */
    decoder->gpu_policy = SIXEL_GPU_POLICY_OFF;
    decoder->dequantize_method = method;
    decoder->dequantize_similarity_bias = similarity;
    decoder->dequantize_edge_strength = edge;
    decoder->dequantize_selective_blur_threshold = threshold;
    status = sixel_decoder_decode_pixels(decoder,
        sixel_chunk_get_buffer(chunk), sixel_chunk_get_size(chunk),
        &options, &result);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_frame_new(&frame, allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_frame_init(frame, result.pixels, result.width,
        result.height, result.pixelformat, NULL, 0);
    if (SIXEL_SUCCEEDED(status)) {
        result.pixels = NULL;
        *out_frame = frame;
        frame = NULL;
    }
cleanup:
    sixel_allocator_free(allocator, result.pixels);
    sixel_frame_unref(frame);
    sixel_decoder_unref(decoder);
    return status;
}

static int
load_dequantized_target_frame(char const *path,
                              sixel_allocator_t *allocator,
                              int dequantize_method,
                              int similarity_bias,
                              int edge_strength,
                              int selective_blur_threshold,
                              int direct, unsigned char const *bg,
                              sixel_frame_t **out_frame)
{
    SIXELSTATUS status;
    sixel_chunk_t *chunk;
    unsigned char const *bytes;
    size_t byte_size;
    unsigned char *indexed_pixels;
    unsigned char *palette;
    unsigned char *rgb_pixels;
    sixel_frame_t *frame;
    int width;
    int height;
    int ncolors;

    status = SIXEL_FALSE;
    chunk = NULL;
    bytes = NULL;
    byte_size = 0u;
    indexed_pixels = NULL;
    palette = NULL;
    rgb_pixels = NULL;
    frame = NULL;
    width = 0;
    height = 0;
    ncolors = 0;

    if (path == NULL || allocator == NULL || out_frame == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto error;
    }
    *out_frame = NULL;

    status = sixel_chunk_create_from_source(&chunk,
                                            path,
                                            0,
                                            NULL,
                                            allocator);
    if (SIXEL_FAILED(status)) {
        goto error;
    }
    bytes = sixel_chunk_get_buffer(chunk);
    byte_size = sixel_chunk_get_size(chunk);
    if (bytes == NULL || byte_size == 0u) {
        sixel_helper_set_additional_message(
            "target input did not provide SIXEL bytes.");
        status = SIXEL_BAD_INPUT;
        goto error;
    }
    if (byte_size > (size_t)INT_MAX) {
        sixel_helper_set_additional_message(
            "target SIXEL input is too large to decode.");
        status = SIXEL_BAD_INPUT;
        goto error;
    }

    if (direct) {
        status = load_direct_chunk(chunk, allocator, bg, dequantize_method,
            similarity_bias, edge_strength, selective_blur_threshold,
            out_frame);
        if (SIXEL_FAILED(status)) {
            goto error;
        }
        chunk->vtbl->unref(chunk);
        return 0;
    }

    /*
     * Dequantization is target-side only and intentionally bypasses the
     * general image loader, matching `sixel2png -d METHOD | lsqa ref -`.
     */
    status = sixel_decode_raw((unsigned char *)bytes,
                              (int)byte_size,
                              &indexed_pixels,
                              &width,
                              &height,
                              &palette,
                              &ncolors,
                              allocator);
    if (SIXEL_FAILED(status)) {
        goto error;
    }
    if (width > SIXEL_WIDTH_LIMIT || height > SIXEL_HEIGHT_LIMIT) {
        sixel_helper_set_additional_message(
            "target SIXEL dimensions exceed the decoder limit.");
        status = SIXEL_BAD_INPUT;
        goto error;
    }

    if (dequantize_method == SIXEL_DEQUANTIZE_K_UNDITHER) {
        status = sixel_dequantize_k_undither(indexed_pixels,
                                             width,
                                             height,
                                             palette,
                                             ncolors,
                                             similarity_bias,
                                             edge_strength,
                                             allocator,
                                             &rgb_pixels);
        if (SIXEL_FAILED(status)) {
            goto error;
        }
    } else if (dequantize_method == SIXEL_DEQUANTIZE_LSO_UNDITHER_VLIGHT) {
        status = sixel_dequantize_k_undither_fast4(indexed_pixels,
                                                   width,
                                                   height,
                                                   palette,
                                                   ncolors,
                                                   similarity_bias,
                                                   allocator,
                                                   &rgb_pixels);
        if (SIXEL_FAILED(status)) {
            goto error;
        }
    } else if (dequantize_method == SIXEL_DEQUANTIZE_SELECTIVE_BLUR) {
        status = sixel_dequantize_selective_blur(indexed_pixels,
                                                 width,
                                                 height,
                                                 palette,
                                                 ncolors,
                                                 selective_blur_threshold,
                                                 allocator,
                                                 &rgb_pixels);
        if (SIXEL_FAILED(status)) {
            goto error;
        }
    } else {
        sixel_helper_set_additional_message(
            "target dequantize method is not supported.");
        status = SIXEL_BAD_ARGUMENT;
        goto error;
    }

    status = sixel_frame_new(&frame, allocator);
    if (SIXEL_FAILED(status)) {
        goto error;
    }
    status = sixel_frame_init(frame,
                              rgb_pixels,
                              width,
                              height,
                              SIXEL_PIXELFORMAT_RGB888,
                              NULL,
                              0);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    rgb_pixels = NULL;
    *out_frame = frame;
    frame = NULL;

    if (chunk != NULL && chunk->vtbl != NULL &&
            chunk->vtbl->unref != NULL) {
        chunk->vtbl->unref(chunk);
    }
    if (indexed_pixels != NULL) {
        sixel_allocator_free(allocator, indexed_pixels);
    }
    if (palette != NULL) {
        sixel_allocator_free(allocator, palette);
    }
    return 0;

error:
    if (SIXEL_FAILED(status)) {
        char const *detail;

        detail = sixel_helper_get_additional_message();
        if (detail != NULL && detail[0] != '\0') {
            fprintf(stderr,
                    "libsixel target dequantize failed for %s: %s (%s)\n",
                    path != NULL ? path : "(null)",
                    sixel_helper_format_error(status),
                    detail);
        } else {
            fprintf(stderr,
                    "libsixel target dequantize failed for %s: %s\n",
                    path != NULL ? path : "(null)",
                    sixel_helper_format_error(status));
        }
    }
    if (frame != NULL) {
        sixel_frame_unref(frame);
    }
    if (rgb_pixels != NULL) {
        sixel_allocator_free(allocator, rgb_pixels);
    }
    if (indexed_pixels != NULL) {
        sixel_allocator_free(allocator, indexed_pixels);
    }
    if (palette != NULL) {
        sixel_allocator_free(allocator, palette);
    }
    if (chunk != NULL && chunk->vtbl != NULL &&
            chunk->vtbl->unref != NULL) {
        chunk->vtbl->unref(chunk);
    }
    return -1;
}

/* Explicit definitions are applied to original SIXEL palette or paint samples.
 * PNG overrides need the pre-CMS loader seam validated in M3. Never attach
 * a source profile to RGB which a legacy loader has already normalized. */
static int
load_defined_frame(char const *path, char const *spec,
                    sixel_allocator_t *allocator, unsigned char const *bg,
                    int direct, int progressive, size_t *passes,
                    sixel_frame_t **out_frame)
{
    sixel_color_def_t *definition;
    sixel_color_def_t *srgb;
    sixel_chunk_t *chunk;
    unsigned char const *bytes;
    unsigned char *indices;
    unsigned char *rgba;
    unsigned char *mask;
    unsigned char *palette;
    unsigned char *profile;
    float *pixels;
    float mapped[256 * 3];
    float background[3];
    double rgb[3];
    double xyz[3];
    double encoded[3];
    char const *icc_path;
    FILE *file;
    long length;
    size_t count;
    size_t i;
    int c;
    int n;
    int width;
    int height;
    int intent;
    unsigned int capture_flags;
    SIXELSTATUS status;
    sixel_frame_t *frame;

    definition = NULL;
    srgb = NULL;
    chunk = NULL;
    indices = NULL;
    rgba = NULL;
    mask = NULL;
    palette = NULL;
    profile = NULL;
    pixels = NULL;
    file = NULL;
    frame = NULL;
    status = SIXEL_BAD_INPUT;
    *out_frame = NULL;
    if (!sixel_cdef_parse(spec, &definition, &icc_path, &intent)) {
        goto cleanup;
    }
    if (icc_path != NULL) {
        file = fopen(icc_path, "rb");
        if (file == NULL || fseek(file, 0, SEEK_END) != 0) {
            goto cleanup;
        }
        length = ftell(file);
        if (length < 128 || length > 16 * 1024 * 1024
                || fseek(file, 0, SEEK_SET) != 0) {
            goto cleanup;
        }
        profile = (unsigned char *)malloc((size_t)length);
        if (profile == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto cleanup;
        }
        if (fread(profile, 1, (size_t)length, file) != (size_t)length) {
            goto cleanup;
        }
        definition = sixel_cdef_icc(profile, (size_t)length);
    }
    srgb = sixel_cdef_name("srgb");
    if (definition == NULL || srgb == NULL) {
        goto cleanup;
    }
    status = sixel_chunk_create_from_source(&chunk, path, 0, NULL, allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    bytes = sixel_chunk_get_buffer(chunk);
    count = sixel_chunk_get_size(chunk);
    status = SIXEL_BAD_INPUT;
    if (bytes == NULL || count < 2 || count > INT_MAX
            || (!progressive && !(bytes[0] == 0x90
                 || (bytes[0] == 0x1b && bytes[1] == 'P')))) {
        sixel_helper_set_additional_message(
            "explicit input colorspace currently requires standalone SIXEL; "
            "PNG overrides need pre-normalization support.");
        goto cleanup;
    }
    n = 0;
    if (progressive) {
        status = sixel_decode_sequence_mapped(bytes, count, NULL,
            &rgba, &width, &height, &capture_flags, passes, allocator);
    } else if (direct) {
        /* Never put transformed colors into an RGB8 palette callback: P3
         * colors can lie outside sRGB. Preserve paint-time RGBA first. */
        status = sixel_decode_direct((unsigned char *)bytes, (int)count,
            &rgba, &width, &height, allocator);
    } else {
        status = sixel_decode_raw_with_options_mask((unsigned char *)bytes,
            (int)count, 0, &indices, &mask, &width, &height, &palette, &n,
            NULL, allocator);
    }
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = SIXEL_BAD_INPUT;
    if (width <= 0 || height <= 0 || width > SIXEL_WIDTH_LIMIT
            || height > SIXEL_HEIGHT_LIMIT
            || (!direct && (n < 1 || n > 256))) {
        goto cleanup;
    }
    count = (size_t)width * (size_t)height;
    if (count / (size_t)width != (size_t)height
            || count > SIZE_MAX / (3 * sizeof(float))) {
        goto cleanup;
    }
    for (i = 0; i < (size_t)n; ++i) {
        for (c = 0; c < 3; ++c) {
            rgb[c] = palette[i * 3 + c] / 255.0;
        }
        if (!sixel_cdef_to_xyz(definition, rgb, xyz, intent)
                || !sixel_cdef_from_xyz(srgb, xyz, encoded, 1)) {
            goto cleanup;
        }
        for (c = 0; c < 3; ++c) {
            mapped[i * 3 + c] = (float)sixel_color_decode_srgb(encoded[c]);
            if (!isfinite(mapped[i * 3 + c])) {
                goto cleanup;
            }
        }
    }
    /* Background is always encoded sRGB, independently of the palette.
     * The mask retains unpainted pixels even with all 256 entries in use. */
    for (c = 0; c < 3; ++c) {
        background[c] = (float)sixel_color_decode_srgb(
            bg == NULL ? 0.0 : bg[c] / 255.0);
    }
    pixels = (float *)sixel_allocator_malloc(allocator,
                                             count * 3 * sizeof(float));
    if (pixels == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto cleanup;
    }
    for (i = 0; i < count; ++i) {
        if (direct) {
            if (rgba[i * 4 + 3] == 0) {
                memcpy(pixels + i * 3, background, sizeof(background));
                continue;
            }
            for (c = 0; c < 3; ++c) {
                rgb[c] = rgba[i * 4 + c] / 255.0;
            }
            if (!sixel_cdef_to_xyz(definition, rgb, xyz, intent)
                    || !sixel_cdef_from_xyz(srgb, xyz, encoded, 1)) {
                goto cleanup;
            }
            for (c = 0; c < 3; ++c) {
                pixels[i * 3 + c] =
                    (float)sixel_color_decode_srgb(encoded[c]);
                if (!isfinite(pixels[i * 3 + c])) {
                    goto cleanup;
                }
            }
        } else {
            if ((mask == NULL || mask[i] != 0) && indices[i] >= n) {
                goto cleanup;
            }
            for (c = 0; c < 3; ++c) {
                pixels[i * 3 + c] = mask != NULL && mask[i] == 0
                    ? background[c] : mapped[(size_t)indices[i] * 3 + c];
            }
        }
    }
    status = sixel_frame_new(&frame, allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_frame_init(frame, (unsigned char *)pixels, width, height,
                              SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, NULL, 0);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    pixels = NULL;
    *out_frame = frame;
    frame = NULL;
cleanup:
    if (file != NULL) {
        fclose(file);
    }
    free(profile);
    sixel_cdef_free(definition);
    sixel_cdef_free(srgb);
    sixel_frame_unref(frame);
    sixel_allocator_free(allocator, pixels);
    sixel_allocator_free(allocator, indices);
    sixel_allocator_free(allocator, rgba);
    sixel_allocator_free(allocator, mask);
    sixel_allocator_free(allocator, palette);
    if (chunk != NULL) {
        chunk->vtbl->unref(chunk);
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "Cannot interpret %s as %s: %s\n", path, spec,
                sixel_helper_get_additional_message());
        return -1;
    }
    return 0;
}

/* Keep format detection local to the opt-in adapter. Other formats still
 * use their original loader and source metadata, including when piped in. */
static int
load_input_frame(char const *path, char const *spec, int direct,
                   sixel_allocator_t *allocator, char const *loader_order,
                   unsigned char *bg, char const **drawing,
                   int progressive, size_t *passes, sixel_frame_t **out_frame)
{
    sixel_chunk_t *chunk;
    unsigned char const *bytes;
    size_t count;
    SIXELSTATUS status;
    int result;

    if (progressive) {
        /* Route before the DCS probe: captures can begin with DECSC. */
        *drawing = "direct";
        return load_defined_frame(path,
            strcmp(spec, "auto") == 0 ? "srgb" : spec, allocator, bg,
            1, 1, passes, out_frame);
    }
    if (strcmp(spec, "auto") != 0) {
        *drawing = direct ? "direct" : "indexed";
        return load_defined_frame(path, spec, allocator, bg, direct, 0, NULL,
                                   out_frame);
    }
    *drawing = "loader-managed";
    if (!direct) {
        return load_frame(path, NULL, allocator, loader_order, bg, out_frame);
    }
    chunk = NULL;
    status = sixel_chunk_create_from_source(&chunk, path, 0, NULL, allocator);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "Cannot read %s: %s\n", path,
                sixel_helper_get_additional_message());
        return -1;
    }
    bytes = sixel_chunk_get_buffer(chunk);
    count = sixel_chunk_get_size(chunk);
    if (count >= 2 && (bytes[0] == 0x90
            || (bytes[0] == 0x1b && bytes[1] == 'P'))) {
        *drawing = "direct";
        status = load_direct_chunk(chunk, allocator, bg,
            SIXEL_DEQUANTIZE_NONE, 100, 0,
            SIXEL_DEQUANTIZE_SELECTIVE_BLUR_THRESHOLD_DEFAULT, out_frame);
        result = SIXEL_FAILED(status) ? -1 : 0;
        if (result != 0) {
            fprintf(stderr, "Cannot decode %s directly: %s\n", path,
                    sixel_helper_get_additional_message());
        }
    } else {
        *drawing = "not-applicable";
        result = load_frame(path, chunk, allocator, loader_order, bg,
                             out_frame);
    }
    chunk->vtbl->unref(chunk);
    return result;
}

typedef struct Metrics {
    float scielab;
    float scielab_max;
    float ms_ssim;
    float high_freq_out;
    float high_freq_ref;
    float high_freq_delta;
    float stripe_ref;
    float stripe_out;
    float stripe_rel;
    float band_run_rel;
    float band_grad_rel;
    float clip_l_ref;
    float clip_r_ref;
    float clip_g_ref;
    float clip_b_ref;
    float clip_l_out;
    float clip_r_out;
    float clip_g_out;
    float clip_b_out;
    float clip_l_rel;
    float clip_r_rel;
    float clip_g_rel;
    float clip_b_rel;
    float delta_chroma_mean;
    float delta_e00_mean;
    float delta_e00_max;
    float gmsd_value;
    float psnr_y;
    float region_mean;
    float region_max;
    unsigned int valid_mask;
} Metrics;

typedef struct MetricBinding {
    const char *name;
    size_t offset;
    int metric_id;
} MetricBinding;

static const MetricBinding g_metric_bindings[] = {
    {"S-CIELAB", offsetof(Metrics, scielab),
     SIXEL_ASSESSMENT_METRIC_SCIELAB},
    {"S-CIELAB_max", offsetof(Metrics, scielab_max),
     SIXEL_ASSESSMENT_METRIC_SCIELAB_MAX},
    {"DE00_region_mean", offsetof(Metrics, region_mean),
     SIXEL_ASSESSMENT_METRIC_REGION_MEAN},
    {"DE00_region_max", offsetof(Metrics, region_max),
     SIXEL_ASSESSMENT_METRIC_REGION_MAX},
    {"MS-SSIM", offsetof(Metrics, ms_ssim),
     SIXEL_ASSESSMENT_METRIC_MS_SSIM},
    {"HighFreqRatio_out", offsetof(Metrics, high_freq_out),
     SIXEL_ASSESSMENT_METRIC_HIGH_FREQ_OUT},
    {"HighFreqRatio_ref", offsetof(Metrics, high_freq_ref),
     SIXEL_ASSESSMENT_METRIC_HIGH_FREQ_REF},
    {"HighFreqRatio_delta", offsetof(Metrics, high_freq_delta),
     SIXEL_ASSESSMENT_METRIC_HIGH_FREQ_DELTA},
    {"StripeScore_ref", offsetof(Metrics, stripe_ref),
     SIXEL_ASSESSMENT_METRIC_STRIPE_REF},
    {"StripeScore_out", offsetof(Metrics, stripe_out),
     SIXEL_ASSESSMENT_METRIC_STRIPE_OUT},
    {"StripeScore_rel", offsetof(Metrics, stripe_rel),
     SIXEL_ASSESSMENT_METRIC_STRIPE_REL},
    {"BandingIndex_rel", offsetof(Metrics, band_run_rel),
     SIXEL_ASSESSMENT_METRIC_BAND_RUN_REL},
    {"BandingIndex_grad_rel", offsetof(Metrics, band_grad_rel),
     SIXEL_ASSESSMENT_METRIC_BAND_GRAD_REL},
    {"ClipRate_L_ref", offsetof(Metrics, clip_l_ref),
     SIXEL_ASSESSMENT_METRIC_CLIP_L_REF},
    {"ClipRate_R_ref", offsetof(Metrics, clip_r_ref),
     SIXEL_ASSESSMENT_METRIC_CLIP_R_REF},
    {"ClipRate_G_ref", offsetof(Metrics, clip_g_ref),
     SIXEL_ASSESSMENT_METRIC_CLIP_G_REF},
    {"ClipRate_B_ref", offsetof(Metrics, clip_b_ref),
     SIXEL_ASSESSMENT_METRIC_CLIP_B_REF},
    {"ClipRate_L_out", offsetof(Metrics, clip_l_out),
     SIXEL_ASSESSMENT_METRIC_CLIP_L_OUT},
    {"ClipRate_R_out", offsetof(Metrics, clip_r_out),
     SIXEL_ASSESSMENT_METRIC_CLIP_R_OUT},
    {"ClipRate_G_out", offsetof(Metrics, clip_g_out),
     SIXEL_ASSESSMENT_METRIC_CLIP_G_OUT},
    {"ClipRate_B_out", offsetof(Metrics, clip_b_out),
     SIXEL_ASSESSMENT_METRIC_CLIP_B_OUT},
    {"ClipRate_L_rel", offsetof(Metrics, clip_l_rel),
     SIXEL_ASSESSMENT_METRIC_CLIP_L_REL},
    {"ClipRate_R_rel", offsetof(Metrics, clip_r_rel),
     SIXEL_ASSESSMENT_METRIC_CLIP_R_REL},
    {"ClipRate_G_rel", offsetof(Metrics, clip_g_rel),
     SIXEL_ASSESSMENT_METRIC_CLIP_G_REL},
    {"ClipRate_B_rel", offsetof(Metrics, clip_b_rel),
     SIXEL_ASSESSMENT_METRIC_CLIP_B_REL},
    {"Δ Chroma_mean", offsetof(Metrics, delta_chroma_mean),
     SIXEL_ASSESSMENT_METRIC_DELTA_CHROMA},
    {"Δ E00_mean", offsetof(Metrics, delta_e00_mean),
     SIXEL_ASSESSMENT_METRIC_DELTA_E00},
    {"DE00_max", offsetof(Metrics, delta_e00_max),
     SIXEL_ASSESSMENT_METRIC_DELTA_E00_MAX},
    {"GMSD", offsetof(Metrics, gmsd_value),
     SIXEL_ASSESSMENT_METRIC_GMSD},
    {"PSNR_Y", offsetof(Metrics, psnr_y),
     SIXEL_ASSESSMENT_METRIC_PSNR_Y},
};

typedef struct MetricSpec {
    const char *option;
    const char *json_key;
    int metric_id;
} MetricSpec;

static const MetricSpec sixel_metric_specs[] = {
    {"S-CIELAB", "S-CIELAB", SIXEL_ASSESSMENT_METRIC_SCIELAB},
    {"S-CIELAB_max", "S-CIELAB_max", SIXEL_ASSESSMENT_METRIC_SCIELAB_MAX},
    {"DE00_region_mean", "DE00_region_mean",
     SIXEL_ASSESSMENT_METRIC_REGION_MEAN},
    {"DE00_region_max", "DE00_region_max",
     SIXEL_ASSESSMENT_METRIC_REGION_MAX},
    {"MS-SSIM", "MS-SSIM", SIXEL_ASSESSMENT_METRIC_MS_SSIM},
    /* Keep established prefixes, including -mm, on the canonical name. */
    {"M", "MS-SSIM", SIXEL_ASSESSMENT_METRIC_MS_SSIM},
    {"MS", "MS-SSIM", SIXEL_ASSESSMENT_METRIC_MS_SSIM},
    {"MS-", "MS-SSIM", SIXEL_ASSESSMENT_METRIC_MS_SSIM},
    {"MS-S", "MS-SSIM", SIXEL_ASSESSMENT_METRIC_MS_SSIM},
    {"MS-SS", "MS-SSIM", SIXEL_ASSESSMENT_METRIC_MS_SSIM},
    {"MS-SSI", "MS-SSIM", SIXEL_ASSESSMENT_METRIC_MS_SSIM},
    {"MS_SSIM", "MS-SSIM", SIXEL_ASSESSMENT_METRIC_MS_SSIM},
    {"SSIM", "MS-SSIM", SIXEL_ASSESSMENT_METRIC_MS_SSIM},
    {"HIGHFREQ_OUT", "HighFreqRatio_out",
     SIXEL_ASSESSMENT_METRIC_HIGH_FREQ_OUT},
    {"HIGHFREQ_REF", "HighFreqRatio_ref",
     SIXEL_ASSESSMENT_METRIC_HIGH_FREQ_REF},
    {"HIGHFREQ_DELTA", "HighFreqRatio_delta",
     SIXEL_ASSESSMENT_METRIC_HIGH_FREQ_DELTA},
    {"STRIPE_REF", "StripeScore_ref",
     SIXEL_ASSESSMENT_METRIC_STRIPE_REF},
    {"STRIPE_OUT", "StripeScore_out",
     SIXEL_ASSESSMENT_METRIC_STRIPE_OUT},
    {"STRIPE_REL", "StripeScore_rel",
     SIXEL_ASSESSMENT_METRIC_STRIPE_REL},
    {"BANDING_REL", "BandingIndex_rel",
     SIXEL_ASSESSMENT_METRIC_BAND_RUN_REL},
    {"BANDING_GRAD_REL", "BandingIndex_grad_rel",
     SIXEL_ASSESSMENT_METRIC_BAND_GRAD_REL},
    {"CLIP_L_REF", "ClipRate_L_ref",
     SIXEL_ASSESSMENT_METRIC_CLIP_L_REF},
    {"CLIP_R_REF", "ClipRate_R_ref",
     SIXEL_ASSESSMENT_METRIC_CLIP_R_REF},
    {"CLIP_G_REF", "ClipRate_G_ref",
     SIXEL_ASSESSMENT_METRIC_CLIP_G_REF},
    {"CLIP_B_REF", "ClipRate_B_ref",
     SIXEL_ASSESSMENT_METRIC_CLIP_B_REF},
    {"CLIP_L_OUT", "ClipRate_L_out",
     SIXEL_ASSESSMENT_METRIC_CLIP_L_OUT},
    {"CLIP_R_OUT", "ClipRate_R_out",
     SIXEL_ASSESSMENT_METRIC_CLIP_R_OUT},
    {"CLIP_G_OUT", "ClipRate_G_out",
     SIXEL_ASSESSMENT_METRIC_CLIP_G_OUT},
    {"CLIP_B_OUT", "ClipRate_B_out",
     SIXEL_ASSESSMENT_METRIC_CLIP_B_OUT},
    {"CLIP_L_REL", "ClipRate_L_rel",
     SIXEL_ASSESSMENT_METRIC_CLIP_L_REL},
    {"CLIP_R_REL", "ClipRate_R_rel",
     SIXEL_ASSESSMENT_METRIC_CLIP_R_REL},
    {"CLIP_G_REL", "ClipRate_G_rel",
     SIXEL_ASSESSMENT_METRIC_CLIP_G_REL},
    {"CLIP_B_REL", "ClipRate_B_rel",
     SIXEL_ASSESSMENT_METRIC_CLIP_B_REL},
    {"DELTA_CHROMA", "Δ Chroma_mean",
     SIXEL_ASSESSMENT_METRIC_DELTA_CHROMA},
    {"DELTA_E00", "Δ E00_mean", SIXEL_ASSESSMENT_METRIC_DELTA_E00},
    {"DE00_max", "DE00_max", SIXEL_ASSESSMENT_METRIC_DELTA_E00_MAX},
    {"DELTA_E00_MAX", "DE00_max", SIXEL_ASSESSMENT_METRIC_DELTA_E00_MAX},
    {"GMSD", "GMSD", SIXEL_ASSESSMENT_METRIC_GMSD},
    {"PSNR_Y", "PSNR_Y", SIXEL_ASSESSMENT_METRIC_PSNR_Y},
};

static void
metrics_init(Metrics *metrics)
{
    memset(metrics, 0, sizeof(*metrics));
    metrics->valid_mask = 0u;
}

static void
metrics_set_value(Metrics *metrics,
                  const char *name,
                  double value,
                  int is_valid)
{
    size_t i;
    unsigned char *base;

    base = (unsigned char *)metrics;
    for (i = 0;
            i < sizeof(g_metric_bindings) / sizeof(g_metric_bindings[0]);
            ++i) {
        if (strcmp(name, g_metric_bindings[i].name) == 0) {
            float *field;
            unsigned int mask;

            field = (float *)(void *)(base + g_metric_bindings[i].offset);
            mask = SIXEL_ASSESSMENT_METRIC_MASK(
                    g_metric_bindings[i].metric_id);
            if (is_valid && isfinite(value)) {
                *field = (float)value;
                metrics->valid_mask |= mask;
            } else {
                *field = 0.0f;
                metrics->valid_mask &= ~mask;
            }
            return;
        }
    }
}

#if !(HAVE__STRCASECMP || HAVE__STRCMPI || HAVE_STRCASECMP)
static int
lsqa_ascii_tolower(int ch)
{
    if (ch >= 'A' && ch <= 'Z') {
        return ch + ('a' - 'A');
    }

    return ch;
}


static int
lsqa_ascii_strcasecmp(const char *lhs, const char *rhs)
{
    int result;
    unsigned char lhs_char;
    unsigned char rhs_char;

    result = 0;
    do {
        lhs_char = (unsigned char)*lhs;
        rhs_char = (unsigned char)*rhs;
        result = lsqa_ascii_tolower((int)lhs_char) -
                 lsqa_ascii_tolower((int)rhs_char);
        if (result != 0 || lhs_char == '\0' || rhs_char == '\0') {
            break;
        }
        ++lhs;
        ++rhs;
    } while (1);

    return result;
}
#endif


static int
metric_name_matches(const char *input, const char *target)
{
    if (input == NULL || target == NULL) {
        return 0;
    }
#if HAVE__STRCASECMP
    if (_stricmp(input, target) == 0) {
        return 1;
    }
#elif HAVE__STRCMPI
    if (_strcmpi(input, target) == 0) {
        return 1;
    }
#elif HAVE_STRCASECMP
    if (strcasecmp(input, target) == 0) {
        return 1;
    }
#else
    if (lsqa_ascii_strcasecmp(input, target) == 0) {
        return 1;
    }
#endif
    return 0;
}

static int
lsqa_ascii_tolower_char(int ch)
{
    if (ch >= 'A' && ch <= 'Z') {
        return ch + ('a' - 'A');
    }

    return ch;
}

static int
lsqa_ascii_strdown_dup(char const *input, char **out_copy)
{
    size_t i;
    size_t length;
    char *copy;

    if (out_copy == NULL) {
        return -1;
    }
    *out_copy = NULL;

    if (input == NULL) {
        return -1;
    }

    length = strlen(input);
    copy = (char *)malloc(length + 1u);
    if (copy == NULL) {
        return -1;
    }
    for (i = 0u; i < length; ++i) {
        copy[i] = (char)lsqa_ascii_tolower_char((unsigned char)input[i]);
    }
    copy[length] = '\0';
    *out_copy = copy;
    return 0;
}

static const MetricSpec *
metric_spec_from_metric_id(int metric_id)
{
    size_t i;

    for (i = 0u; i < sizeof(sixel_metric_specs) / sizeof(sixel_metric_specs[0]);
         ++i) {
        if (sixel_metric_specs[i].metric_id == metric_id) {
            return &sixel_metric_specs[i];
        }
    }
    return NULL;
}

static int
lsqa_parse_metric_spec_choice(char const *name,
                              MetricSpec const **out_spec,
                              char *detail,
                              size_t detail_size)
{
    size_t spec_count;
    size_t choice_capacity;
    size_t choice_count;
    size_t i;
    char *normalized_name;
    char **choice_names;
    sixel_option_choice_t *choices;
    sixel_option_choice_result_t match_result;
    int matched_metric_id;
    char match_detail[256];
    char message[256];
    int status;
    int wrote;
    char const *detail_source;

    if (detail != NULL && detail_size > 0u) {
        detail[0] = '\0';
    }
    if (out_spec != NULL) {
        *out_spec = NULL;
    }
    if (name == NULL || name[0] == '\0' || out_spec == NULL) {
        if (detail != NULL && detail_size > 0u) {
            (void)snprintf(detail, detail_size, "%s", "Unknown metric name.");
        }
        return -1;
    }

    spec_count = sizeof(sixel_metric_specs) / sizeof(sixel_metric_specs[0]);
    choice_capacity = spec_count * 2u;
    normalized_name = NULL;
    choice_names = NULL;
    choices = NULL;
    choice_count = 0u;
    status = -1;
    matched_metric_id = 0;
    match_detail[0] = '\0';
    message[0] = '\0';

    if (lsqa_ascii_strdown_dup(name, &normalized_name) != 0) {
        goto cleanup;
    }

    choice_names = (char **)calloc(choice_capacity, sizeof(*choice_names));
    choices = (sixel_option_choice_t *)calloc(choice_capacity, sizeof(*choices));
    if (choice_names == NULL || choices == NULL) {
        goto cleanup;
    }

    for (i = 0u; i < spec_count; ++i) {
        if (lsqa_ascii_strdown_dup(sixel_metric_specs[i].option,
                                   &choice_names[choice_count]) != 0) {
            goto cleanup;
        }
        choices[choice_count].name = choice_names[choice_count];
        choices[choice_count].value = sixel_metric_specs[i].metric_id;
        ++choice_count;

        if (lsqa_ascii_strdown_dup(sixel_metric_specs[i].json_key,
                                   &choice_names[choice_count]) != 0) {
            goto cleanup;
        }
        choices[choice_count].name = choice_names[choice_count];
        choices[choice_count].value = sixel_metric_specs[i].metric_id;
        ++choice_count;
    }

    match_result = sixel_option_match_choice(normalized_name,
                                             choices,
                                             choice_count,
                                             &matched_metric_id,
                                             match_detail,
                                             sizeof(match_detail));
    if (match_result == SIXEL_OPTION_CHOICE_MATCH) {
        *out_spec = metric_spec_from_metric_id(matched_metric_id);
        if (*out_spec == NULL) {
            goto cleanup;
        }
        status = 0;
        goto cleanup;
    }

    if (match_result == SIXEL_OPTION_CHOICE_AMBIGUOUS) {
        sixel_option_report_ambiguous_prefix(normalized_name,
                                             match_detail,
                                             message,
                                             sizeof(message));
    } else {
        sixel_option_report_invalid_choice("Unknown metric name.",
                                           match_detail,
                                           message,
                                           sizeof(message));
    }
    if (detail != NULL && detail_size > 0u) {
        detail_source = sixel_helper_get_additional_message();
        if ((detail_source == NULL || detail_source[0] == '\0')
                && message[0] != '\0') {
            detail_source = message;
        }
        if (detail_source != NULL && detail_source[0] != '\0') {
            wrote = snprintf(detail, detail_size, "%s", detail_source);
            if (wrote < 0) {
                detail[0] = '\0';
            }
        }
    }

cleanup:
    if (choice_names != NULL) {
        for (i = 0u; i < choice_count; ++i) {
            free(choice_names[i]);
        }
    }
    free(choices);
    free(choice_names);
    free(normalized_name);
    return status;
}

static const MetricSpec *
metric_spec_from_name(const char *name)
{
    size_t i;

    if (name == NULL) {
        return NULL;
    }
    for (i = 0;
            i < sizeof(sixel_metric_specs) / sizeof(sixel_metric_specs[0]);
            ++i) {
        if (metric_name_matches(name, sixel_metric_specs[i].option) ||
                metric_name_matches(name,
                sixel_metric_specs[i].json_key)) {
            return &sixel_metric_specs[i];
        }
    }
    return NULL;
}

static int
metrics_get_value(const Metrics *metrics,
                  const char *name,
                  float *out_value)
{
    size_t i;
    const unsigned char *base;
    unsigned int mask;

    if (metrics == NULL || name == NULL || out_value == NULL) {
        return -1;
    }
    base = (const unsigned char *)metrics;
    mask = 0u;
    for (i = 0; i < sizeof(g_metric_bindings) / sizeof(g_metric_bindings[0]);
            ++i) {
        if (strcmp(name, g_metric_bindings[i].name) == 0) {
            const float *field;

            field = (const float *)(const void *)(base +
                    g_metric_bindings[i].offset);
            mask = SIXEL_ASSESSMENT_METRIC_MASK(
                    g_metric_bindings[i].metric_id);
            if ((metrics->valid_mask & mask) == 0u) {
                return 1;
            }
            *out_value = *field;
            return 0;
        }
    }
    return -1;
}

static unsigned int
metrics_mask_for_name(const char *name)
{
    size_t i;

    if (name == NULL) {
        return 0u;
    }
    for (i = 0; i < sizeof(g_metric_bindings) / sizeof(g_metric_bindings[0]);
            ++i) {
        if (strcmp(name, g_metric_bindings[i].name) == 0) {
            return SIXEL_ASSESSMENT_METRIC_MASK(
                    g_metric_bindings[i].metric_id);
        }
    }
    return 0u;
}

typedef struct MetricItem {
    const char *name;
    float value;
} MetricItem;

static void
verbose_print(const Metrics *m)
{
    MetricItem items[] = {
        {"S-CIELAB", m->scielab},
        {"S-CIELAB_max", m->scielab_max},
        {"MS-SSIM", m->ms_ssim},
        {"HighFreqRatio_out", m->high_freq_out},
        {"HighFreqRatio_ref", m->high_freq_ref},
        {"HighFreqRatio_delta", m->high_freq_delta},
        {"StripeScore_ref", m->stripe_ref},
        {"StripeScore_out", m->stripe_out},
        {"StripeScore_rel", m->stripe_rel},
        {"BandingIndex_rel", m->band_run_rel},
        {"BandingIndex_grad_rel", m->band_grad_rel},
        {"ClipRate_L_ref", m->clip_l_ref},
        {"ClipRate_R_ref", m->clip_r_ref},
        {"ClipRate_G_ref", m->clip_g_ref},
        {"ClipRate_B_ref", m->clip_b_ref},
        {"ClipRate_L_out", m->clip_l_out},
        {"ClipRate_R_out", m->clip_r_out},
        {"ClipRate_G_out", m->clip_g_out},
        {"ClipRate_B_out", m->clip_b_out},
        {"ClipRate_L_rel", m->clip_l_rel},
        {"ClipRate_R_rel", m->clip_r_rel},
        {"ClipRate_G_rel", m->clip_g_rel},
        {"ClipRate_B_rel", m->clip_b_rel},
        {"Δ Chroma_mean", m->delta_chroma_mean},
        {"Δ E00_mean", m->delta_e00_mean},
        {"DE00_max", m->delta_e00_max},
        {"GMSD", m->gmsd_value},
        {"PSNR_Y", m->psnr_y},
    };
    size_t i;

    fprintf(stderr, "\n=== Image Quality Report (Raw Metrics) ===\n");
    for (i = 0; i < sizeof(items) / sizeof(items[0]); ++i) {
        unsigned int mask;

        mask = metrics_mask_for_name(items[i].name);
        if (mask == 0u || (m->valid_mask & mask) == 0u) {
            fprintf(stderr, "%24s: NaN\n", items[i].name);
        } else {
            fprintf(stderr, "%24s: %.6f\n",
                    items[i].name,
                    items[i].value);
        }
    }
}

typedef struct JsonCollector {
    FILE *stream;
    Metrics *metrics;
} JsonCollector;

static void
json_collect_callback(char const *chunk, size_t length, void *user_data)
{
    JsonCollector *collector;
    char buffer[256];
    char *start;
    char *end;
    char *colon;
    char name[64];
    double value;
    int is_valid;

    collector = (JsonCollector *)user_data;
    if (collector->stream != NULL) {
        if (fwrite(chunk, 1, length, collector->stream) != length) {
            {
                char errbuf[128];

                fprintf(stderr,
                        "Failed to write JSON output: %s\n",
                        lsqa_strerror(errno, errbuf,
                                      sizeof(errbuf)));
            }
        }
    }
    if (collector->metrics == NULL) {
        return;
    }
    if (length >= sizeof(buffer)) {
        length = sizeof(buffer) - 1u;
    }
    memcpy(buffer, chunk, length);
    buffer[length] = '\0';
    start = strchr(buffer, '"');
    if (start == NULL) {
        return;
    }
    end = strchr(start + 1, '"');
    if (end == NULL) {
        return;
    }
    colon = strchr(end, ':');
    if (colon == NULL) {
        return;
    }
    colon += 1;
    while (*colon == ' ') {
        colon += 1;
    }
    value = 0.0;
    is_valid = 0;
    if (strncmp(colon, "NaN", 3) == 0) {
        is_valid = 0;
    } else if (strncmp(colon, "null", 4) == 0) {
        is_valid = 0;
    } else {
        char *tail;

        value = strtod(colon, &tail);
        if (tail == colon) {
            return;
        }
        is_valid = 1;
    }
    if ((size_t)(end - start - 1) >= sizeof(name)) {
        return;
    }
    memcpy(name, start + 1, (size_t)(end - start - 1));
    name[end - start - 1] = '\0';
    metrics_set_value(collector->metrics, name, value, is_valid);
}

enum {
    LSQA_COMPARE_COLORSPACE_REFERENCE = (-1)
};

typedef enum LsqaComparePrecision {
    LSQA_COMPARE_PRECISION_REFERENCE = 0,
    LSQA_COMPARE_PRECISION_8BIT = 1,
    LSQA_COMPARE_PRECISION_FLOAT32 = 2
} LsqaComparePrecision;

static sixel_option_choice_t const g_lsqa_compare_colorspace_choices[] = {
    { "reference", LSQA_COMPARE_COLORSPACE_REFERENCE },
    { "gamma", SIXEL_COLORSPACE_GAMMA },
    { "linear", SIXEL_COLORSPACE_LINEAR },
    { "oklab", SIXEL_COLORSPACE_OKLAB },
    { "cielab", SIXEL_COLORSPACE_CIELAB },
    { "din99d", SIXEL_COLORSPACE_DIN99D }
};

static sixel_option_choice_t const g_lsqa_compare_precision_choices[] = {
    { "reference", LSQA_COMPARE_PRECISION_REFERENCE },
    { "8bit", LSQA_COMPARE_PRECISION_8BIT },
    { "float32", LSQA_COMPARE_PRECISION_FLOAT32 }
};

static char const g_lsqa_compare_colorspace_detail[] =
    "compare-colorspace accepts reference, gamma, linear, oklab, "
    "cielab, or din99d.";

static char const g_lsqa_compare_precision_detail[] =
    "compare-precision accepts reference, 8bit, or float32.";

static char const g_lsqa_dequantize_detail[] =
    "dequantize accepts none, k_undither, lso_undither:Vfs, "
    "lso_undither:Vlight, selective_blur:threshold=N, "
    "or short forms l:Vf, l:Vl, and s:TN.";

static void
lsqa_copy_parse_detail(char *detail,
                       size_t detail_size,
                       char const *fallback)
{
    char const *source;
    int written;

    if (detail == NULL || detail_size == 0u) {
        return;
    }
    detail[0] = '\0';

    source = sixel_helper_get_additional_message();
    if ((source == NULL || source[0] == '\0')
            && fallback != NULL
            && fallback[0] != '\0') {
        source = fallback;
    }
    if (source == NULL || source[0] == '\0') {
        return;
    }
    written = snprintf(detail, detail_size, "%s", source);
    if (written < 0) {
        detail[0] = '\0';
    }
}

static int
lsqa_parse_choice_argument(char const *argument,
                           sixel_option_choice_t const *choices,
                           size_t choice_count,
                           char const *invalid_detail,
                           int *out_value,
                           char *detail,
                           size_t detail_size)
{
    sixel_option_choice_result_t match_result;
    int matched_value;
    char match_detail[256];
    char message[256];

    if (detail != NULL && detail_size > 0u) {
        detail[0] = '\0';
    }
    if (argument == NULL || argument[0] == '\0'
            || out_value == NULL
            || choices == NULL
            || choice_count == 0u) {
        if (detail != NULL && detail_size > 0u
                && invalid_detail != NULL
                && invalid_detail[0] != '\0') {
            (void) snprintf(detail, detail_size, "%s", invalid_detail);
        }
        return -1;
    }

    matched_value = 0;
    match_detail[0] = '\0';
    message[0] = '\0';
    match_result = sixel_option_match_choice(argument,
                                             choices,
                                             choice_count,
                                             &matched_value,
                                             match_detail,
                                             sizeof(match_detail));
    if (match_result == SIXEL_OPTION_CHOICE_MATCH) {
        *out_value = matched_value;
        return 0;
    }

    if (match_result == SIXEL_OPTION_CHOICE_AMBIGUOUS) {
        sixel_option_report_ambiguous_prefix(argument,
                                             match_detail,
                                             message,
                                             sizeof(message));
    } else {
        sixel_option_report_invalid_choice(invalid_detail,
                                           match_detail,
                                           message,
                                           sizeof(message));
    }
    lsqa_copy_parse_detail(detail, detail_size, message);
    return -1;
}

static int
lsqa_precision_from_pixelformat(int pixelformat)
{
    if (SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat)) {
        return LSQA_COMPARE_PRECISION_FLOAT32;
    }
    return LSQA_COMPARE_PRECISION_8BIT;
}

static int
lsqa_parse_compare_colorspace(char const *argument,
                              int *out_colorspace,
                              char *detail,
                              size_t detail_size)
{
    return lsqa_parse_choice_argument(argument,
                                      g_lsqa_compare_colorspace_choices,
                                      sizeof(g_lsqa_compare_colorspace_choices)
                                      / sizeof(g_lsqa_compare_colorspace_choices[0]),
                                      g_lsqa_compare_colorspace_detail,
                                      out_colorspace,
                                      detail,
                                      detail_size);
}

/* Values index canonical assignments applied after all --env options. */
static sixel_option_choice_t const g_lsqa_cms_choices[] = {
    { "none", 0 }, { "off", 0 }, { "disabled", 0 },
    { "auto", 1 }, { "builtin", 2 },
    { "lcms2", 3 }, { "lcms", 3 },
    { "colorsync", 4 }, { "color-sync", 4 }
};

static char const *const g_lsqa_cms_assignments[] = {
    "SIXEL_LOADER_CMS_ENGINE=none",
    "SIXEL_LOADER_CMS_ENGINE=auto",
    "SIXEL_LOADER_CMS_ENGINE=builtin",
    "SIXEL_LOADER_CMS_ENGINE=lcms2",
    "SIXEL_LOADER_CMS_ENGINE=colorsync"
};

static int
lsqa_parse_compare_precision(char const *argument,
                             int *out_precision,
                             char *detail,
                             size_t detail_size)
{
    return lsqa_parse_choice_argument(argument,
                                      g_lsqa_compare_precision_choices,
                                      sizeof(g_lsqa_compare_precision_choices)
                                      / sizeof(g_lsqa_compare_precision_choices[0]),
                                      g_lsqa_compare_precision_detail,
                                      out_precision,
                                      detail,
                                      detail_size);
}

static int
lsqa_parse_dequantize_method(char const *argument,
                             int *out_method,
                             int *out_selective_blur_threshold,
                             char *detail,
                             size_t detail_size)
{
    SIXELSTATUS status;
    char const *detail_source;

    if (detail != NULL && detail_size > 0u) {
        detail[0] = '\0';
    }
    if (argument == NULL || argument[0] == '\0' || out_method == NULL) {
        lsqa_copy_parse_detail(detail,
                               detail_size,
                               g_lsqa_dequantize_detail);
        return -1;
    }

    status = sixel_option_parse_dequantize_argument_with_options(
        argument,
        out_method,
        out_selective_blur_threshold,
        NULL,
        0u);
    if (SIXEL_FAILED(status)) {
        detail_source = sixel_helper_get_additional_message();
        if (detail_source == NULL || detail_source[0] == '\0') {
            detail_source = g_lsqa_dequantize_detail;
        }
        lsqa_copy_parse_detail(detail, detail_size, detail_source);
        return -1;
    }

    return 0;
}

static int
lsqa_parse_dequantize_bias(char const *argument,
                           int *out_bias,
                           char *detail,
                           size_t detail_size,
                           char const *name)
{
    long parsed_value;
    char *endptr;

    parsed_value = 0L;
    endptr = NULL;
    if (detail != NULL && detail_size > 0u) {
        detail[0] = '\0';
    }
    if (argument == NULL || argument[0] == '\0' || out_bias == NULL) {
        if (detail != NULL && detail_size > 0u) {
            (void)snprintf(detail,
                           detail_size,
                           "%s requires an integer between 0 and 1000.",
                           name);
        }
        return -1;
    }

    errno = 0;
    parsed_value = strtol(argument, &endptr, 10);
    if (endptr == argument || endptr[0] != '\0' || errno == ERANGE ||
            parsed_value < 0L || parsed_value > 1000L) {
        if (detail != NULL && detail_size > 0u) {
            (void)snprintf(detail,
                           detail_size,
                           "%s must be an integer between 0 and 1000.",
                           name);
        }
        return -1;
    }

    *out_bias = (int)parsed_value;
    return 0;
}

static int
lsqa_parse_boolean(char const *argument,
                   int *out_value)
{
    if (argument == NULL || argument[0] == '\0' || out_value == NULL) {
        return -1;
    }

    if (metric_name_matches(argument, "1")
            || metric_name_matches(argument, "true")
            || metric_name_matches(argument, "yes")
            || metric_name_matches(argument, "on")) {
        *out_value = 1;
        return 0;
    }
    if (metric_name_matches(argument, "0")
            || metric_name_matches(argument, "false")
            || metric_name_matches(argument, "no")
            || metric_name_matches(argument, "off")) {
        *out_value = 0;
        return 0;
    }

    return -1;
}

static int
lsqa_parse_bgcolor(char const *argument,
                   unsigned char *bgcolor,
                   char *detail,
                   size_t detail_size)
{
    SIXELSTATUS status;
    char const *detail_source;
    int written;

    status = SIXEL_FALSE;
    detail_source = NULL;
    written = 0;

    if (detail != NULL && detail_size > 0u) {
        detail[0] = '\0';
    }
    if (argument == NULL || argument[0] == '\0' || bgcolor == NULL) {
        if (detail != NULL && detail_size > 0u) {
            (void)snprintf(detail,
                           detail_size,
                           "%s",
                           "cannot parse bgcolor option.");
        }
        return -1;
    }

    /*
     * Reuse the OSC11/img2sixel colorspec parser to keep -B grammar aligned.
     */
    status = sixel_tty_parse_colorspec(bgcolor, argument);
    if (SIXEL_FAILED(status)) {
        detail_source = sixel_helper_get_additional_message();
        if (detail_source == NULL || detail_source[0] == '\0') {
            detail_source = "cannot parse bgcolor option.";
        }
        if (detail != NULL && detail_size > 0u) {
            written = snprintf(detail,
                               detail_size,
                               "%s",
                               detail_source);
            if (written < 0) {
                detail[0] = '\0';
            }
        }
        return -1;
    }

    return 0;
}

static int
lsqa_copy_option_text(char *buffer,
                      size_t buffer_size,
                      char const *value)
{
    int written;

    if (buffer == NULL || buffer_size == 0u || value == NULL) {
        return -1;
    }
    written = snprintf(buffer, buffer_size, "%s", value);
    if (written < 0 || (size_t)written >= buffer_size) {
        return -1;
    }
    return 0;
}

/* A gate owns its metric and direction; argument order never pairs -m/-b. */
typedef struct Baseline {
    const MetricSpec *spec;
    double value;
    int maximum;
} Baseline;

typedef struct Options {
    const char *ref_spec;
    const char *out_spec;
    double scielab_ppd;
    int region_window;
    int direct;
    int progressive_input;
    int animation_input;
    char frame_report[PATH_MAX];
    char target_timeline[PATH_MAX];
    char error_map[PATH_MAX];
    char display_timeline[PATH_MAX];
    int clock_alignment;
    double persistence_time;
    double scielab_threshold;
    double reference_fps;
    unsigned int video_specified;
    const char *ref_path;
    const char *out_path;
    char loader_order_buffer[PATH_MAX];
    const MetricSpec *metric_spec;
    const char *metric_key;
    Baseline baselines[2 * SIXEL_ASSESSMENT_METRIC_COUNT];
    unsigned int report_mask;
    unsigned int baseline_mask;
    unsigned int metric_mask;
    int report_count;
    int baseline_count;
    int verbose;
    int grayscale_compare;
    int compare_colorspace;
    int compare_precision;
    int dequantize_method;
    int dequantize_similarity_bias;
    int dequantize_edge_strength;
    int dequantize_selective_blur_threshold;
    const char *loader_order;
    int grayscale_specified;
    int compare_colorspace_specified;
    int compare_precision_specified;
    int loader_order_specified;
    unsigned char bgcolor[3];
    int has_bgcolor;
} Options;

/* Explicit CLI values win over environment defaults, including zero. */
static int
lsqa_video_option(Options *opts, int option, char const *value)
{
    static sixel_option_choice_t const alignments[] = {
        {"frame", 0}, {"clock", 1}
    };
    char *path;
    char *end;
    double number;
    double denominator;
    unsigned int bit;

    path = NULL;
    bit = 0;
    switch (option) {
    case 'o': path = opts->frame_report; bit = 1u; break;
    case 'J': path = opts->target_timeline; bit = 2u; break;
    case 'E': path = opts->error_map; bit = 4u; break;
    case 'u': bit = 8u; break;
    case 'f': bit = 16u; break;
    case 'C': path = opts->display_timeline; bit = 32u; break;
    case 'A': bit = 64u; break;
    case 't': bit = 128u; break;
    default: return -1;
    }
    if (value == NULL || value[0] == '\0') { return -1; }
    if (option == 'A') {
        if (sixel_option_match_choice(value, alignments, 2,
                &opts->clock_alignment, NULL, 0)
                != SIXEL_OPTION_CHOICE_MATCH) { return -1; }
    } else if (path != NULL) {
        if (strcmp(value, "-") == 0
                || lsqa_copy_option_text(path, PATH_MAX, value) != 0) {
            return -1;
        }
    } else {
        errno = 0;
        number = strtod(value, &end);
        if (end == value || errno != 0 || !isfinite(number)
                || number < 0) { return -1; }
        if (option == 'f' && *end == '/') {
            value = end + 1;
            errno = 0;
            denominator = strtod(value, &end);
            if (end == value || errno != 0 || !isfinite(denominator)
                    || denominator <= 0) { return -1; }
            number /= denominator;
        }
        if (*end != '\0' || !isfinite(number)) { return -1; }
        if (option == 'f') {
            if (number <= 0 || number > 1000) { return -1; }
            opts->reference_fps = number;
        } else if (option == 't') {
            opts->persistence_time = number;
        } else {
            opts->scielab_threshold = number;
        }
    }
    opts->video_specified |= bit;
    return 0;
}

static int
lsqa_video_environment(Options *opts)
{
    static char const *const names[] = {
        "LSQA_FRAME_REPORT", "LSQA_TARGET_TIMELINE", "LSQA_ERROR_MAP",
        "LSQA_SCIELAB_THRESHOLD", "LSQA_REFERENCE_FPS",
        "LSQA_DISPLAY_TIMELINE", "LSQA_ALIGNMENT", "LSQA_PERSISTENCE_TIME"
    };
    static char const options[] = "oJEufCAt";
    char *value;
    int i;
    int failed;

    for (i = 0; i < 8; ++i) {
        if ((opts->video_specified & (1u << i)) != 0) { continue; }
        value = lsqa_getenv_dup(names[i]);
        failed = value != NULL && value[0] != '\0'
            && lsqa_video_option(opts, options[i], value) != 0;
        free(value);
        if (failed) {
            sixel_helper_set_additional_message(names[i]);
            return -1;
        }
    }
    return 0;
}

static void
lsqa_report_invalid_argument(int short_opt,
                             char const *value,
                             char const *detail);

static void
lsqa_set_parse_error(char const *message);

static int
lsqa_parse_selection(const char *arg, Options *opts, int baseline);

static int
lsqa_apply_env_overrides(Options *opts)
{
    char *metrics_env;
    char *baseline_env;
    char *grayscale_env;
    char *colorspace_env;
    char *precision_env;
    char *loaders_env;
    int boolean_value;
    int status;
    char option_detail[256];

    if (opts == NULL) {
        return -1;
    }

    metrics_env = NULL;
    baseline_env = NULL;
    grayscale_env = NULL;
    colorspace_env = NULL;
    precision_env = NULL;
    loaders_env = NULL;
    boolean_value = 0;
    status = 0;
    option_detail[0] = '\0';

    metrics_env = lsqa_getenv_dup("LSQA_METRICS");
    baseline_env = lsqa_getenv_dup("LSQA_BASELINE");
    grayscale_env = lsqa_getenv_dup("LSQA_GRAYSCALE");
    colorspace_env = lsqa_getenv_dup("LSQA_COMPARE_COLORSPACE");
    precision_env = lsqa_getenv_dup("LSQA_COMPARE_PRECISION");
    loaders_env = lsqa_getenv_dup("LSQA_LOADERS");

    if (metrics_env != NULL && metrics_env[0] != '\0'
            && !opts->report_count
            && lsqa_parse_selection(metrics_env, opts, 0) != 0) {
        status = -1;
        goto cleanup;
    }
    if (baseline_env != NULL && baseline_env[0] != '\0'
            && !opts->baseline_count
            && lsqa_parse_selection(baseline_env, opts, 1) != 0) {
        status = -1;
        goto cleanup;
    }

    if (grayscale_env != NULL && grayscale_env[0] != '\0'
            && !opts->grayscale_specified) {
        if (lsqa_parse_boolean(grayscale_env, &boolean_value) != 0) {
            lsqa_report_invalid_argument('g',
                                         grayscale_env,
                                         "Expected 0/1 or false/true.");
            status = -1;
            goto cleanup;
        }
        opts->grayscale_compare = boolean_value;
    }

    if (colorspace_env != NULL && colorspace_env[0] != '\0'
            && !opts->compare_colorspace_specified) {
        if (lsqa_parse_compare_colorspace(colorspace_env,
                                          &opts->compare_colorspace,
                                          option_detail,
                                          sizeof(option_detail)) != 0) {
            lsqa_report_invalid_argument(
                'W',
                colorspace_env,
                option_detail[0] != '\0'
                    ? option_detail
                    : g_lsqa_compare_colorspace_detail);
            status = -1;
            goto cleanup;
        }
    }

    if (precision_env != NULL && precision_env[0] != '\0'
            && !opts->compare_precision_specified) {
        if (lsqa_parse_compare_precision(precision_env,
                                         &opts->compare_precision,
                                         option_detail,
                                         sizeof(option_detail)) != 0) {
            lsqa_report_invalid_argument(
                'P',
                precision_env,
                option_detail[0] != '\0'
                    ? option_detail
                    : g_lsqa_compare_precision_detail);
            status = -1;
            goto cleanup;
        }
    }

    if (loaders_env != NULL && loaders_env[0] != '\0'
            && !opts->loader_order_specified) {
        if (lsqa_copy_option_text(opts->loader_order_buffer,
                                  sizeof(opts->loader_order_buffer),
                                  loaders_env) != 0) {
            lsqa_report_invalid_argument('L',
                                         loaders_env,
                                         "Loader list is too long.");
            status = -1;
            goto cleanup;
        }
        opts->loader_order = opts->loader_order_buffer;
    }

cleanup:
    free(loaders_env);
    free(precision_env);
    free(colorspace_env);
    free(grayscale_env);
    free(baseline_env);
    free(metrics_env);
    return status;
}

/*
 * Per-option help text for --help output and contextual errors.
 * The layout mirrors converter CLI tables to keep behavior consistent.
 */
static cli_option_help_t const g_option_help_table[] = {
    {
        'M', "input-mode",
        "-M MODE, --input-mode=MODE  target: single (default) or\n"
        "                           6delta-progressive (one still), or\n"
        "                           animation (video assessment).\n"
        "                           Unique prefixes: -Ms, -M6, -Ma.\n"
    },
    {'A', "alignment",
        "-A MODE, --alignment=MODE  frame (default) or clock; prefixes OK.\n"
        "                           Clock requires -C and excludes -J.\n"
        "                           Env: LSQA_ALIGNMENT.\n"},
    {'C', "display-timeline",
        "-C FILE, --display-timeline=FILE  update/byte_end/pts JSONL.\n"
        "                           Complete-packet availability on clock.\n"
        "                           Env: LSQA_DISPLAY_TIMELINE.\n"},
    {'t', "persistence-time",
        "-t SECONDS, --persistence-time=SECONDS  error-weight ramp (-o).\n"
        "                           Default 0.1; 0 gives immediate weight.\n"
        "                           Env: LSQA_PERSISTENCE_TIME.\n"},
    {'o', "frame-report",
        "-o FILE, --frame-report=FILE  frame/clock-segment JSONL (-Ma).\n"
        "                           Env: LSQA_FRAME_REPORT.\n"},
    {'J', "target-timeline",
        "-J FILE, --target-timeline=FILE  frame/byte_end/pts/duration JSONL.\n"
        "                           Default: one DCS per reference frame.\n"
        "                           Env: LSQA_TARGET_TIMELINE.\n"},
    {'E', "error-map",
        "-E FILE, --error-map=FILE  raw float32-le S-CIELAB maps; needs -o.\n"
        "                           Env: LSQA_ERROR_MAP.\n"},
    {'u', "scielab-threshold",
        "-u VALUE, --scielab-threshold=VALUE  report errors strictly above\n"
        "                           VALUE (default 2, diagnostic CIE76).\n"
        "                           Env: LSQA_SCIELAB_THRESHOLD.\n"},
    {'f', "reference-fps",
        "-f RATE, --reference-fps=RATE  override loader frame durations.\n"
        "                           Positive FPS or ratio, at most 1000.\n"
        "                           Default: loader centiseconds or -J.\n"
        "                           Env: LSQA_REFERENCE_FPS.\n"},
    {
        'D', "direct",
        "-D, --direct              render both SIXEL inputs at paint time.\n"
        "                           Independent of -R/-T color definitions.\n"
        "                           Default indexed; other formats unchanged.\n"
    },
    {
        'm',
        "metrics",
        "-m NAME[,NAME...], --metrics=NAME[,NAME...] (repeatable)\n"
        "                           one unique metric: value; multiple: JSON.\n"
        "                           Gate-only metrics are not printed.\n"
    },
    {
        'b',
        "baseline",
        "-b METRIC:min=VALUE, --baseline=METRIC:max=VALUE (repeatable)\n"
        "                           comma lists; all bounds must pass.\n"
        "                           METRIC:VALUE retains legacy directions:\n"
        "                           minimum except S-CIELAB/S-CIELAB_max.\n"
    },
    {
        '%',
        "env",
        "-% KEY=VALUE, --env=KEY=VALUE\n"
        "                           set process environment variable\n"
        "                           before assessment. Repeatable.\n"
    },
    {
        'g',
        "grayscale",
        "-g, --grayscale            grayscale signal; "
        "color metrics unchanged.\n"
    },
    {
        'R', "reference-colorspace",
        "-R SPEC, --reference-colorspace=SPEC (default auto).\n"
        "                           Original input definition: auto, srgb,\n"
        "                           display-p3, adobe-rgb, dci-p3,\n"
        "                           icc:[INTENT:]FILE, or\n"
        "                           rgb:wx,wy,rx,ry,gx,gy,bx,by:srgb or\n"
        "                           rgb:wx,wy,rx,ry,gx,gy,bx,by:gamma=r,g,b.\n"
        "                           INTENT: perceptual, relative (default),\n"
        "                           saturation. Explicit: SIXEL only.\n"
    },
    {
        'T', "target-colorspace",
        "-T SPEC, --target-colorspace=SPEC (default auto).\n"
        "                           Independent target input definition.\n"
    },
    {
        'v', "scielab-ppd",
        "-v PPD, --scielab-ppd=PPD: pixels/degree, 1..1024 (default 60).\n"
        "                           S-CIELAB and S-CIELAB_max use CIE76.\n"
        "                           Lower is better; zero is identical.\n"
    },
    {
        'w', "region-window",
        "-w SIZE, --region-window=SIZE: 8, 16 (default), 32, 64.\n"
        "                           Fixed XYZ mean tiles with partial edges.\n"
        "                           Metrics: DE00_region_mean,\n"
        "                           DE00_region_max.\n"
    },
    {
        'W',
        "compare-colorspace",
        "-W COLORSPACE, --compare-colorspace=COLORSPACE\n"
        "                           luma signal basis: reference, gamma,\n"
        "                           linear, oklab, cielab, din99d.\n"
        "                           Gamma uses encoded luma; the other\n"
        "                           spaces use linear Y. Color metrics\n"
        "                           keep the original physical colors.\n"
    },
    {
        'P',
        "compare-precision",
        "-P PRECISION, --compare-precision=PRECISION\n"
        "                           signal precision (reference,\n"
        "                           8bit, float32).\n"
    },
    {
        'L',
        "loaders",
        "-L LIST, --loaders=LIST    override loader order (same syntax\n"
        "                           as img2sixel -L). libpng supports\n"
        "                           :chrm_arithmetic=auto|double|512bit\n"
        "                           (:Avalue, default auto), limited to\n"
        "                           its selected cHRM matrix. Environment:\n"
        "                           SIXEL_LOADER_LIBPNG_CHRM_ARITHMETIC.\n"
    },
    {
        '#',
        "cms-engine",
        "-# ENGINE, --cms-engine=ENGINE\n"
        "                           loader CMS for both inputs:\n"
        "                           none/auto/builtin/lcms2/colorsync.\n"
        "                           Overrides SIXEL_LOADER_CMS_ENGINE;\n"
        "                           loader-specific settings take priority.\n"
        "                           Default: auto; explicit env is retained.\n"
    },
    {
        'B',
        "bgcolor",
        "-B BGCOLOR, --bgcolor=BGCOLOR\n"
        "                           specify background color.\n"
        "                           BGCOLOR syntax follows img2sixel:\n"
        "                             #rgb\n"
        "                             #rrggbb\n"
        "                             #rrrgggbbb\n"
        "                             #rrrrggggbbbb\n"
        "                             rgb:r/g/b\n"
        "                             rgb:rr/gg/bb\n"
        "                             rgb:rrr/ggg/bbb\n"
        "                             rgb:rrrr/gggg/bbbb\n"
    },
    {
        'd',
        "dequantize",
        "-d METHOD, --dequantize=METHOD\n"
        "                           dequantize target SIXEL input\n"
        "                           before comparison.\n"
        "                           METHOD is one of:\n"
        "                             none              -> disable\n"
        "                             k_undither        -> Kornelski's\n"
        "                                                  undither\n"
        "                             lso_undither:Vfs\n"
        "                             l:Vf              -> full FS undither\n"
        "                             lso_undither:Vlight\n"
        "                             l:Vl              -> light causal\n"
        "                                                  undither\n"
        "                             selective_blur:threshold=24\n"
        "                                               -> selective\n"
        "                                                  3x3 blur\n"
        "                             s:T24             -> compact\n"
        "                                                  3x3 blur\n"
    },
    {
        'S',
        "similarity",
        "-S BIAS, --similarity=BIAS specify dequantize similarity\n"
        "                           bias, range: 0-1000\n"
        "                           (default: 100).\n"
    },
    {
        'e',
        "edge",
        "-e BIAS, --edge=BIAS       specify dequantize edge\n"
        "                           protection bias, range: 0-1000\n"
        "                           (default: 0).\n"
    },
    {
        'H',
        "help",
        "-H, --help                 show this help.\n"
    }
};

static char const g_option_help_fallback[] =
    "    Refer to \"lsqa -H\" for more details.\n";

static size_t
lsqa_option_help_count(void)
{
    return sizeof(g_option_help_table) /
        sizeof(g_option_help_table[0]);
}

static char const g_lsqa_optstring[] =
    "DM:R:T:w:v:m:b:%:gW:P:L:#:B:d:S:e:o:J:E:u:f:A:C:t:Hh";

static void
lsqa_print_option_help(FILE *stream)
{
    cli_print_option_help(stream,
                          g_option_help_table,
                          lsqa_option_help_count());
}

static void
lsqa_report_missing_argument(int short_opt)
{
    cli_report_missing_argument("lsqa",
                                g_option_help_fallback,
                                g_option_help_table,
                                lsqa_option_help_count(),
                                short_opt);
}

static void
lsqa_report_missing_argument_callback(int short_opt, void *user_data)
{
    (void)user_data;

    lsqa_report_missing_argument(short_opt);
}

static void
lsqa_report_unrecognized_option(int short_opt, char const *token)
{
    cli_report_unrecognized_option("lsqa", short_opt, token);
}

static void
lsqa_handle_getopt_error(int short_opt, char const *token)
{
    cli_option_help_t const *entry;
    cli_option_help_t const *long_entry;
    char const *long_name;

    entry = NULL;
    long_entry = NULL;
    long_name = NULL;

    if (short_opt > 0) {
        entry = cli_find_option_help(g_option_help_table,
                                     lsqa_option_help_count(),
                                     short_opt);
        if (entry != NULL) {
            lsqa_report_missing_argument(short_opt);
            return;
        }
    }

    if (token != NULL && token[0] != '\0') {
        if (strncmp(token, "--", 2) == 0) {
            long_name = token + 2;
        } else if (token[0] == '-') {
            long_name = token + 1;
        }
        if (long_name != NULL && long_name[0] != '\0') {
            long_entry = cli_find_option_help_by_long_name(
                g_option_help_table,
                lsqa_option_help_count(),
                long_name);
            if (long_entry != NULL) {
                lsqa_report_missing_argument(long_entry->short_opt);
                return;
            }
        }
    }

    lsqa_report_unrecognized_option(short_opt, token);
}

static int
lsqa_guard_missing_argument(int short_opt, char *const *argv)
{
    return cli_guard_missing_argument(short_opt,
                                      argv,
                                      optarg,
                                      &optind,
                                      g_lsqa_optstring,
                                      g_option_help_table,
                                      lsqa_option_help_count(),
                                      NULL,
                                      NULL,
                                      lsqa_report_missing_argument_callback,
                                      NULL);
}

static void
lsqa_report_invalid_argument(int short_opt,
                             char const *value,
                             char const *detail)
{
    char buffer[1024];
    char detail_copy[1024];
    cli_option_help_t const *entry;
    char const *long_opt;
    char const *help_text;
    char const *argument;
    size_t offset;
    int written;

    memset(buffer, 0, sizeof(buffer));
    memset(detail_copy, 0, sizeof(detail_copy));
    entry = cli_find_option_help(g_option_help_table,
                                 lsqa_option_help_count(),
                                 short_opt);
    long_opt = (entry != NULL && entry->long_opt != NULL)
        ? entry->long_opt : "?";
    help_text = (entry != NULL && entry->help != NULL)
        ? entry->help : g_option_help_fallback;
    argument = (value != NULL && value[0] != '\0')
        ? value : "(missing)";
    offset = 0u;

    written = snprintf(buffer,
                       sizeof(buffer),
                       "\\fW'%s'\\fP is invalid argument for "
                       "\\fB-%c\\fP,\\fB--%s\\fP option:\n\n",
                       argument,
                       (char)short_opt,
                       long_opt);
    if (written < 0) {
        written = 0;
    }
    if ((size_t)written >= sizeof(buffer)) {
        offset = sizeof(buffer) - 1u;
    } else {
        offset = (size_t)written;
    }

    if (detail != NULL && detail[0] != '\0' && offset < sizeof(buffer) - 1u) {
        (void) snprintf(detail_copy,
                        sizeof(detail_copy),
                        "%s\n",
                        detail);
        written = snprintf(buffer + offset,
                           sizeof(buffer) - offset,
                           "%s",
                           detail_copy);
        if (written < 0) {
            written = 0;
        }
        if ((size_t)written >= sizeof(buffer) - offset) {
            offset = sizeof(buffer) - 1u;
        } else {
            offset += (size_t)written;
        }
    }

    if (offset < sizeof(buffer) - 1u) {
        written = snprintf(buffer + offset,
                           sizeof(buffer) - offset,
                           "%s",
                           help_text);
        if (written < 0) {
            written = 0;
        }
    }

    sixel_helper_set_additional_message(buffer);
}

static void
print_usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s [-m NAMES]... [-b CONDITIONS]... [-g]\n"
            "             [-W COLORSPACE] [-P PRECISION] "
            "[-L LIST] [-# ENGINE]\n"
            "             [-B BGCOLOR] [-d METHOD] [-S BIAS] [-e BIAS]\n"
            "             <reference> [target]\n",
            prog);
    fprintf(stderr,
            "       %s [-m NAMES]... [-b CONDITIONS]... [-g]\n"
            "             [-W COLORSPACE] [-P PRECISION] "
            "[-L LIST] [-# ENGINE]\n"
            "             [-B BGCOLOR] [-d METHOD] [-S BIAS] [-e BIAS]\n"
            "             <reference> < target\n",
            prog);
    fprintf(stderr,
            "       %s [-m NAMES]... [-b CONDITIONS]... [-g]\n"
            "             [-W COLORSPACE] [-P PRECISION] "
            "[-L LIST] [-# ENGINE]\n"
            "             [-B BGCOLOR] [-d METHOD] [-S BIAS] [-e BIAS]\n"
            "             <reference> - < target\n",
            prog);
    fprintf(stderr,
    "  -m, --metrics NAMES  comma list; repeat to add metrics\n");
    fprintf(stderr,
            "                        one metric: value; multiple: JSON\n");
    fprintf(stderr,
            "  -b, --baseline METRIC:min=VALUE or METRIC:max=VALUE\n");
    fprintf(stderr,
            "                        repeat or use commas; AND all bounds\n"
            "                        each bound names its metric; -m optional\n"
            "                        METRIC:VALUE keeps its legacy direction\n"
            "                        (minimum except S-CIELAB/S-CIELAB_max)\n");
    fprintf(stderr,
            "  -v, --scielab-ppd PPD  pixels/degree, 1..1024 (default 60)\n");
    fprintf(stderr,
            "  -g, --grayscale     compare in grayscale\n");
    fprintf(stderr,
            "  -W, --compare-colorspace COLORSPACE\n");
    fprintf(stderr,
            "                        reference/gamma/linear/oklab/\n"
            "                        cielab/din99d\n");
    fprintf(stderr,
            "  -P, --compare-precision PRECISION\n");
    fprintf(stderr,
            "                        reference/8bit/float32\n");
    fprintf(stderr,
            "  -L, --loaders LIST  loader order (img2sixel -L syntax)\n");
    fprintf(stderr,
            "  -#, --cms-engine ENGINE\n"
            "                        none/auto/builtin/lcms2/colorsync\n");
    fprintf(stderr,
            "  -B, --bgcolor BGCOLOR\n");
    fprintf(stderr,
            "                        background color for alpha composition\n");
    fprintf(stderr,
            "  -D, --direct       paint-time colors for both SIXEL inputs\n");
    fprintf(stderr,
            "  -d, --dequantize METHOD\n");
    fprintf(stderr,
            "                        dequantize target SIXEL input\n");
    fprintf(stderr,
            "  -S, --similarity BIAS\n");
    fprintf(stderr,
            "                        dequantize similarity bias\n");
    fprintf(stderr,
            "  -e, --edge BIAS  dequantize edge protection bias\n");
}

static void
show_help(void)
{
    /*
     * Help text must go to stdout to match converter tools and allow piping.
     */
    fprintf(stdout,
            "Usage: lsqa [-m NAMES]... [-b CONDITIONS]... [-g]\n"
            "            [-W COLORSPACE] [-P PRECISION] [-L LIST] [-# ENGINE]\n"
            "            [-B BGCOLOR] [-d METHOD] [-S BIAS] [-e BIAS]\n"
            "            <reference> [target]\n"
            "       lsqa [-m NAMES]... [-b CONDITIONS]... [-g]\n"
            "            [-W COLORSPACE] [-P PRECISION] [-L LIST] [-# ENGINE]\n"
            "            [-B BGCOLOR] [-d METHOD] [-S BIAS] [-e BIAS]\n"
            "            <reference> < target\n"
            "       lsqa [-m NAMES]... [-b CONDITIONS]... [-g]\n"
            "            [-W COLORSPACE] [-P PRECISION] [-L LIST] [-# ENGINE]\n"
            "            [-B BGCOLOR] [-d METHOD] [-S BIAS] [-e BIAS]\n"
            "            <reference> - < target\n"
            "\n"
            "Options:\n");
    lsqa_print_option_help(stdout);
    fprintf(stdout,
            "\n"
            "MS-SSIM metric:\n"
            "  MS-SSIM         reference five-scale product (>=176x176)\n"
            "                  unavailable results print nan; JSON uses null\n"
            "\n"
            "Environment overrides:\n"
            "  LSQA_METRICS=NAME[,NAME...] (CLI replaces the list)\n"
            "  LSQA_BASELINE=CONDITION[,CONDITION...] (CLI replaces the list)\n"
            "  LSQA_GRAYSCALE=0|1\n"
            "  LSQA_COMPARE_COLORSPACE=reference|gamma|linear|oklab|"
            "cielab|din99d\n"
            "  LSQA_COMPARE_PRECISION=reference|8bit|float32\n"
            "  LSQA_LOADERS=LIST\n"
            "  SIXEL_LOADER_CMS_ENGINE=none|auto|builtin|lcms2|colorsync\n"
            "  LSQA_VERBOSE=0|1\n"
            "\n"
            "Exit codes:\n"
            "  0  success\n"
            "  2  invalid arguments\n"
            "  3  failed to load input images\n"
            "  4  assessment or output failure\n"
            "  5  baseline threshold failed\n");
}

static void
lsqa_set_parse_error(char const *message)
{
    if (message == NULL || message[0] == '\0') {
        return;
    }
    sixel_helper_set_additional_message(message);
}

static int
lsqa_parse_baseline(const char *arg, Options *opts)
{
    const char *colon;
    size_t name_len;
    const MetricSpec *spec;
    const char *value_text;
    char name[64];
    char *tail;
    double value;
    Baseline *prior;
    Baseline *gate;
    int maximum;
    int explicit_direction;
    int i;

    colon = strchr(arg, ':');
    if (colon == NULL) {
        lsqa_report_invalid_argument('b', arg,
                                     "Expected METRIC:VALUE format.");
        return -1;
    }
    name_len = (size_t)(colon - arg);
    if (name_len == 0u || name_len >= sizeof(name)) {
        lsqa_report_invalid_argument('b', arg,
                                     "Metric name is empty or too long.");
        return -1;
    }
    memcpy(name, arg, name_len);
    name[name_len] = '\0';
    spec = metric_spec_from_name(name);
    if (spec == NULL) {
        lsqa_report_invalid_argument('b', arg, "Unknown metric name.");
        return -1;
    }

    /* Preserve legacy METRIC:VALUE directions. Explicit min/max can gate
     * any metric, including lower-is-better color errors, unambiguously. */
    maximum = spec->metric_id == SIXEL_ASSESSMENT_METRIC_SCIELAB
        || spec->metric_id == SIXEL_ASSESSMENT_METRIC_SCIELAB_MAX;
    explicit_direction = 0;
    value_text = colon + 1;
    if (strncmp(value_text, "min=", 4) == 0
            || strncmp(value_text, "max=", 4) == 0) {
        maximum = value_text[1] == 'a';
        explicit_direction = 1;
        value_text += 4;
    }
    if (value_text[0] == '\0') {
        lsqa_report_invalid_argument('b', arg, "Baseline value is empty.");
        return -1;
    }
    errno = 0;
    value = strtod(value_text, &tail);
    if (tail == value_text) {
        lsqa_report_invalid_argument('b', arg,
                                     "Baseline value is not a number.");
        return -1;
    }
    while (*tail == ' ' || *tail == '\t') {
        ++tail;
    }
    if (*tail != '\0') {
        lsqa_report_invalid_argument('b', arg,
                                     "Unexpected characters in value.");
        return -1;
    }
    if (!isfinite(value) || errno == ERANGE) {
        lsqa_report_invalid_argument('b', arg,
                "Baseline value is not finite or in range.");
        return -1;
    }
    /* Legacy thresholds were stored as float; keep their rounding. New
     * explicit limits retain the supplied double for range validation. */
    if (!explicit_direction) {
        if (value > FLT_MAX || value < -FLT_MAX) {
            lsqa_report_invalid_argument('b', arg,
                                         "Baseline value is out of range.");
            return -1;
        }
        value = (float)value;
    }
    for (i = 0; i < opts->baseline_count; ++i) {
        prior = &opts->baselines[i];
        if (prior->spec->metric_id != spec->metric_id) {
            continue;
        }
        if (prior->maximum == maximum) {
            lsqa_report_invalid_argument('b', arg,
                "baseline already specified for this metric and direction.");
            return -1;
        }
        /* The prior bound has the opposite direction: require min <= max. */
        if (maximum ? value < prior->value : value > prior->value) {
            lsqa_report_invalid_argument('b', arg,
                                         "Baseline minimum exceeds maximum.");
            return -1;
        }
    }
    if (opts->baseline_count >= 2 * SIXEL_ASSESSMENT_METRIC_COUNT) {
        lsqa_report_invalid_argument('b', arg, "Too many baseline limits.");
        return -1;
    }
    gate = &opts->baselines[opts->baseline_count++];
    gate->spec = spec;
    gate->value = value;
    gate->maximum = maximum;
    opts->baseline_mask |= SIXEL_ASSESSMENT_METRIC_MASK(spec->metric_id);
    return 0;
}

/* Repeated options and comma lists have identical additive semantics.
 * Reject empty members instead of silently dropping a missing condition. */
static int
lsqa_parse_selection(const char *arg, Options *opts, int baseline)
{
    char *copy;
    char *item;
    char *next;
    char *end;
    const MetricSpec *spec;
    unsigned int bit;
    int status;
    char detail[256];

    copy = (char *)malloc(strlen(arg) + 1u);
    if (copy == NULL) {
        lsqa_set_parse_error("Cannot allocate metric selection.");
        return -1;
    }
    strcpy(copy, arg);
    status = 0;
    item = copy;
    do {
        next = strchr(item, ',');
        if (next != NULL) {
            *next++ = '\0';
        }
        while (*item == ' ' || *item == '\t') {
            ++item;
        }
        end = item + strlen(item);
        while (end > item && (end[-1] == ' ' || end[-1] == '\t')) {
            *--end = '\0';
        }
        if (*item == '\0') {
            lsqa_report_invalid_argument(baseline ? 'b' : 'm', arg,
                                         "Empty selection member.");
            status = -1;
        } else if (baseline) {
            status = lsqa_parse_baseline(item, opts);
        } else {
            detail[0] = '\0';
            spec = NULL;
            status = lsqa_parse_metric_spec_choice(item, &spec,
                                                   detail, sizeof(detail));
            if (status != 0 || spec == NULL) {
                lsqa_report_invalid_argument('m', item, detail);
                status = -1;
            } else {
                bit = SIXEL_ASSESSMENT_METRIC_MASK(spec->metric_id);
                if ((opts->report_mask & bit) == 0u) {
                    opts->report_mask |= bit;
                    ++opts->report_count;
                    opts->metric_spec = spec;
                    opts->metric_key = spec->json_key;
                }
            }
        }
        item = next;
    } while (status == 0 && item != NULL);
    free(copy);
    return status;
}

/* Report every failed gate, even when an earlier bound already failed.
 * Missing/nonfinite required results cannot satisfy a quality gate. */
static int
lsqa_check_baselines(const Options *opts, const Metrics *metrics)
{
    const Baseline *gate;
    float value;
    int i;
    int status;
    int failed;

    failed = 0;
    for (i = 0; i < opts->baseline_count; ++i) {
        gate = &opts->baselines[i];
        value = 0.0f;
        status = metrics_get_value(metrics, gate->spec->json_key, &value);
        if (status != 0 || !isfinite(value)) {
            fprintf(stderr, "Metric %s is NaN or unavailable\n",
                    gate->spec->json_key);
            failed = 1;
        } else if (gate->maximum ? value > gate->value : value < gate->value) {
            fprintf(stderr, "%s %.6f is %s baseline %.6f\n",
                    gate->spec->json_key, value,
                    gate->maximum ? "above" : "below", gate->value);
            failed = 1;
        }
    }
    return failed ? LSQA_EXIT_BASELINE_FAILED : LSQA_EXIT_SUCCESS;
}

/* One selected metric keeps the scalar interface. Multiple selections use
 * named JSON fields in registry order; gate-only metrics stay off stdout. */
static int
lsqa_print_selection(const Options *opts, const Metrics *metrics,
                     double const *video_means)
{
    size_t i;
    float value;
    double displayed;
    int status;
    int emitted;
    const MetricBinding *binding;

    if (!opts->report_count) {
        return LSQA_EXIT_SUCCESS;
    }
    if (opts->report_count == 1) {
        status = metrics_get_value(metrics, opts->metric_key, &value);
        if (status < 0) {
            return LSQA_EXIT_RUNTIME_FAILED;
        }
        if (status > 0 || !isfinite(value)) {
            printf("nan\n");
        } else {
            displayed = video_means == NULL ? value
                : video_means[SIXEL_ASSESSMENT_INDEX(
                    opts->metric_spec->metric_id)];
            printf("%.6f\n", displayed);
        }
    } else {
        emitted = 0;
        printf("{\n");
        for (i = 0; i < sizeof(g_metric_bindings) /
                sizeof(g_metric_bindings[0]); ++i) {
            binding = &g_metric_bindings[i];
            if ((opts->report_mask & SIXEL_ASSESSMENT_METRIC_MASK(
                    binding->metric_id)) == 0u) {
                continue;
            }
            status = metrics_get_value(metrics, binding->name, &value);
            /* Invalid bindings are runtime errors, as in scalar output.
             * Only unavailable metric values are represented as null. */
            if (status < 0) {
                return LSQA_EXIT_RUNTIME_FAILED;
            }
            printf("%s  \"%s\": ", emitted ? ",\n" : "", binding->name);
            if (status > 0 || !isfinite(value)) {
                printf("null");
            } else {
                displayed = video_means == NULL ? value
                    : video_means[SIXEL_ASSESSMENT_INDEX(binding->metric_id)];
                printf("%.6f", displayed);
            }
            emitted = 1;
        }
        printf("\n}\n");
    }
    return fflush(stdout) == 0 ? LSQA_EXIT_SUCCESS : LSQA_EXIT_RUNTIME_FAILED;
}

static int
lsqa_build_reordered_argv(int argc,
                          char **argv,
                          char ***out_argv,
                          int *out_argc)
{
    char **reordered;
    char **options;
    char **positionals;
    int opt_count;
    int pos_count;
    int end_of_options;
    int i;
    int short_opt;
    int needs_arg;
    int known;
    int new_argc;
    char *token;
    char *next_token;
    char *equals;
    size_t token_len;

    reordered = NULL;
    options = NULL;
    positionals = NULL;
    opt_count = 0;
    pos_count = 0;
    end_of_options = 0;
    new_argc = 0;

    if (out_argv != NULL) {
        *out_argv = NULL;
    }
    if (out_argc != NULL) {
        *out_argc = 0;
    }

    if (argc <= 0 || argv == NULL) {
        return -1;
    }

    reordered = (char **)calloc((size_t)argc + 1u, sizeof(char *));
    options = (char **)calloc((size_t)argc, sizeof(char *));
    positionals = (char **)calloc((size_t)argc, sizeof(char *));
    if (reordered == NULL || options == NULL || positionals == NULL) {
        free(reordered);
        free(options);
        free(positionals);
        lsqa_set_parse_error("lsqa: out of memory while parsing args.");
        return -1;
    }

    for (i = 1; i < argc; ++i) {
        token = argv[i];
        if (token == NULL) {
            continue;
        }
        if (end_of_options != 0) {
            positionals[pos_count++] = token;
            continue;
        }
        if (strcmp(token, "--") == 0) {
            end_of_options = 1;
            continue;
        }
        if (token[0] == '-' && token[1] != '\0') {
            short_opt = 0;
            known = cli_token_is_known_option(g_option_help_table,
                                              lsqa_option_help_count(),
                                              token,
                                              &short_opt);
            if (known == 0) {
                lsqa_report_unrecognized_option(0, token);
                goto error;
            }
            needs_arg = cli_option_requires_argument(g_lsqa_optstring,
                                                     short_opt);
            if (needs_arg != 0) {
                token_len = strlen(token);
                equals = strchr(token, '=');
                if (token[1] == (char)short_opt && token_len > 2u) {
                    options[opt_count++] = token;
                    continue;
                }
                if (equals != NULL) {
                    options[opt_count++] = token;
                    continue;
                }
                if (i + 1 >= argc) {
                    lsqa_report_missing_argument(short_opt);
                    goto error;
                }
                next_token = argv[i + 1];
                if (next_token == NULL) {
                    lsqa_report_missing_argument(short_opt);
                    goto error;
                }
                if (next_token[0] == '-' && next_token[1] != '\0') {
                    known = cli_token_is_known_option(
                        g_option_help_table,
                        lsqa_option_help_count(),
                        next_token,
                        NULL);
                    if (known != 0) {
                        lsqa_report_missing_argument(short_opt);
                        goto error;
                    }
                }
                options[opt_count++] = token;
                options[opt_count++] = next_token;
                i += 1;
                continue;
            }
            options[opt_count++] = token;
            continue;
        }
        positionals[pos_count++] = token;
    }

    new_argc = 1 + opt_count + pos_count;
    reordered[0] = argv[0];
    for (i = 0; i < opt_count; ++i) {
        reordered[1 + i] = options[i];
    }
    for (i = 0; i < pos_count; ++i) {
        reordered[1 + opt_count + i] = positionals[i];
    }
    reordered[new_argc] = NULL;

    if (out_argv != NULL) {
        *out_argv = reordered;
    }
    if (out_argc != NULL) {
        *out_argc = new_argc;
    }

    free(options);
    free(positionals);
    return 0;

error:
    free(reordered);
    free(options);
    free(positionals);
    return -1;
}

static int
lsqa_valid_color_spec(char const *spec)
{
    sixel_color_def_t *definition;
    char const *path;
    int intent;
    int ok;

    definition = NULL;
    ok = sixel_cdef_parse(spec, &definition, &path, &intent);
    sixel_cdef_free(definition);
    return ok;
}

static int
parse_args(int argc, char **argv, Options *opts)
{
    static sixel_option_choice_t const video_mode[] = {{"animation", 1}};
    const char *ref_arg;
    const char *out_arg;
    char *verbose_env;
    int argi;
    int parse_status;
    char **scan_argv;
    int scan_argc;
    int opt;
    int verbose_value;
    int cms_choice;
    char *ppd_end;
    sixel_suboption_value_t input_mode;
    char detail_buffer[256];
#if HAVE_GETOPT_LONG
    int long_opt;
    int option_index;
#endif

    opts->ref_spec = "auto";
    opts->out_spec = "auto";
    opts->scielab_ppd = SIXEL_QA_SCIELAB_PPD_DEFAULT;
    opts->region_window = 16;
    opts->ref_path = NULL;
    opts->out_path = "-";
    opts->metric_spec = NULL;
    opts->metric_key = NULL;
    opts->report_mask = 0u;
    opts->baseline_mask = 0u;
    opts->metric_mask = SIXEL_ASSESSMENT_METRIC_MASK_ALL;
    opts->report_count = 0;
    opts->baseline_count = 0;
    opts->verbose = 0;
    opts->grayscale_compare = 0;
    opts->compare_colorspace = LSQA_COMPARE_COLORSPACE_REFERENCE;
    opts->compare_precision = LSQA_COMPARE_PRECISION_REFERENCE;
    opts->direct = 0;
    opts->progressive_input = 0;
    opts->animation_input = 0;
    opts->frame_report[0] = '\0';
    opts->target_timeline[0] = '\0';
    opts->error_map[0] = '\0';
    opts->display_timeline[0] = '\0';
    opts->clock_alignment = 0;
    opts->persistence_time = 0.1;
    opts->scielab_threshold = 2.0;
    opts->reference_fps = 0;
    opts->video_specified = 0;
    opts->dequantize_method = SIXEL_DEQUANTIZE_NONE;
    opts->dequantize_similarity_bias = 100;
    opts->dequantize_edge_strength = 0;
    opts->dequantize_selective_blur_threshold =
        SIXEL_DEQUANTIZE_SELECTIVE_BLUR_THRESHOLD_DEFAULT;
    opts->loader_order = NULL;
    opts->grayscale_specified = 0;
    opts->compare_colorspace_specified = 0;
    opts->compare_precision_specified = 0;
    opts->loader_order_specified = 0;
    opts->loader_order_buffer[0] = '\0';
    opts->bgcolor[0] = 0u;
    opts->bgcolor[1] = 0u;
    opts->bgcolor[2] = 0u;
    opts->has_bgcolor = 0;

    argi = 1;
    parse_status = 0;
    scan_argv = NULL;
    scan_argc = 0;
    opt = 0;
    verbose_value = 0;
    cms_choice = -1;
    detail_buffer[0] = '\0';
    verbose_env = NULL;
#if HAVE_GETOPT_LONG
    long_opt = 0;
    option_index = 0;
#endif

    sixel_option_apply_cli_suggestion_defaults();

    if (lsqa_build_reordered_argv(argc, argv,
                                  &scan_argv,
                                  &scan_argc) != 0) {
        parse_status = -1;
        goto cleanup;
    }

    optind = 1;

    for (;;) {
#if HAVE_GETOPT_LONG
        struct option long_options[] = {
            {"direct", no_argument, &long_opt, 'D'},
            {"input-mode", required_argument, &long_opt, 'M'},
            {"alignment", required_argument, &long_opt, 'A'},
            {"display-timeline", required_argument, &long_opt, 'C'},
            {"persistence-time", required_argument, &long_opt, 't'},
            {"frame-report", required_argument, &long_opt, 'o'},
            {"target-timeline", required_argument, &long_opt, 'J'},
            {"error-map", required_argument, &long_opt, 'E'},
            {"scielab-threshold", required_argument, &long_opt, 'u'},
            {"reference-fps", required_argument, &long_opt, 'f'},
            {"reference-colorspace", required_argument, &long_opt, 'R'},
            {"target-colorspace", required_argument, &long_opt, 'T'},
            {"region-window", required_argument, &long_opt, 'w'},
            {"scielab-ppd", required_argument, &long_opt, 'v'},
            {"metrics", required_argument, &long_opt, 'm'},
            {"baseline", required_argument, &long_opt, 'b'},
            {"env", required_argument, &long_opt, '%'},
            {"grayscale", no_argument, &long_opt, 'g'},
            {"compare-colorspace", required_argument, &long_opt, 'W'},
            {"compare-precision", required_argument, &long_opt, 'P'},
            {"cms-engine", required_argument, &long_opt, '#'},
            {"loaders", required_argument, &long_opt, 'L'},
            {"bgcolor", required_argument, &long_opt, 'B'},
            {"dequantize", required_argument, &long_opt, 'd'},
            {"similarity", required_argument, &long_opt, 'S'},
            {"edge", required_argument, &long_opt, 'e'},
            {"help", no_argument, &long_opt, 'H'},
            {0, 0, 0, 0}
        };

        opt = getopt_long(scan_argc, scan_argv, g_lsqa_optstring,
                          long_options, &option_index);
#else
        opt = getopt(scan_argc, scan_argv, g_lsqa_optstring);
#endif
        if (opt == -1) {
            break;
        }
#if HAVE_GETOPT_LONG
        if (opt == 0) {
            opt = long_opt;
        }
#endif

        if (opt > 0) {
            if (lsqa_guard_missing_argument(opt, argv) != 0) {
                parse_status = -1;
                goto cleanup;
            }
        }

        switch (opt) {
        case 'A':
        case 'C':
        case 't':
        case 'o':
        case 'J':
        case 'E':
        case 'u':
        case 'f':
            if (lsqa_video_option(opts, opt, optarg) != 0) {
                lsqa_report_invalid_argument(opt, optarg,
                    "Expected a named file or a finite value in range.");
                parse_status = -1;
                goto cleanup;
            }
            break;
        case 'M':
            /* Keep still modes on the shared decoder registry. Animation
             * uses the same prefix matcher but is an lsqa operation. */
            if (sixel_option_match_choice(optarg, video_mode, 1,
                    &opts->animation_input, NULL, 0)
                    == SIXEL_OPTION_CHOICE_MATCH) {
                opts->progressive_input = 0;
                break;
            }
            /* Share the decoder's vocabulary and prefix matching rules. */
            if (SIXEL_FAILED(sixel_option_parse_scalar_argument(
                    SIXEL_OPTION_SCHEMA_DECODE_INPUT_MODE,
                    SIXEL_OPTION_SCOPE_DECODER, optarg, &input_mode,
                    detail_buffer, sizeof(detail_buffer)))) {
                lsqa_report_invalid_argument('M', optarg, detail_buffer);
                parse_status = -1;
                goto cleanup;
            }
            opts->progressive_input = input_mode.int_value;
            opts->animation_input = 0;
            break;
        case 'D':
            opts->direct = 1;
            break;
        case 'R':
            opts->ref_spec = optarg;
            break;
        case 'T':
            opts->out_spec = optarg;
            break;
        case 'v':
            errno = 0;
            opts->scielab_ppd = strtod(optarg, &ppd_end);
            if (errno != 0 || ppd_end == optarg || *ppd_end != '\0'
                    || !isfinite(opts->scielab_ppd)
                    || opts->scielab_ppd < SIXEL_QA_SCIELAB_PPD_MIN
                    || opts->scielab_ppd > SIXEL_QA_SCIELAB_PPD_MAX) {
                lsqa_report_invalid_argument('v', optarg,
                    "expected finite pixels/degree in 1..1024");
                parse_status = -1;
                goto cleanup;
            }
            break;
        case 'w':
            if (strcmp(optarg, "8") != 0 && strcmp(optarg, "16") != 0
                    && strcmp(optarg, "32") != 0
                    && strcmp(optarg, "64") != 0) {
                lsqa_set_parse_error("region-window expects 8, 16, 32 or 64");
                parse_status = -1;
                goto cleanup;
            }
            opts->region_window = atoi(optarg);
            break;
        case 'H':
        case 'h':
            parse_status = 1;
            goto cleanup;
        case '%':
            if (cli_apply_env_assignment(optarg,
                                         detail_buffer,
                                         sizeof(detail_buffer)) != 0) {
                lsqa_set_parse_error(detail_buffer);
                parse_status = -1;
                goto cleanup;
            }
            break;
        case 'g':
            opts->grayscale_compare = 1;
            opts->grayscale_specified = 1;
            break;
        case 'W':
            if (lsqa_parse_compare_colorspace(
                        optarg,
                        &opts->compare_colorspace,
                        detail_buffer,
                        sizeof(detail_buffer)) != 0) {
                lsqa_report_invalid_argument(
                    'W',
                    optarg,
                    detail_buffer[0] != '\0'
                        ? detail_buffer
                        : g_lsqa_compare_colorspace_detail);
                parse_status = -1;
                goto cleanup;
            }
            opts->compare_colorspace_specified = 1;
            break;
        case 'P':
            if (lsqa_parse_compare_precision(
                        optarg,
                        &opts->compare_precision,
                        detail_buffer,
                        sizeof(detail_buffer)) != 0) {
                lsqa_report_invalid_argument(
                    'P',
                    optarg,
                    detail_buffer[0] != '\0'
                        ? detail_buffer
                        : g_lsqa_compare_precision_detail);
                parse_status = -1;
                goto cleanup;
            }
            opts->compare_precision_specified = 1;
            break;
        case '#':
            if (lsqa_parse_choice_argument(
                    optarg, g_lsqa_cms_choices,
                    sizeof(g_lsqa_cms_choices)
                        / sizeof(g_lsqa_cms_choices[0]),
                    "Expected none, auto, builtin, lcms2, or colorsync.",
                    &cms_choice, detail_buffer,
                    sizeof(detail_buffer)) != 0) {
                lsqa_report_invalid_argument('#', optarg, detail_buffer);
                parse_status = -1;
                goto cleanup;
            }
            break;
        case 'L':
            if (opts->loader_order != NULL) {
                lsqa_set_parse_error(
                    "lsqa: loader order already specified.");
                parse_status = -1;
                goto cleanup;
            }
            opts->loader_order = optarg;
            opts->loader_order_specified = 1;
            break;
        case 'B':
            if (lsqa_parse_bgcolor(optarg,
                                   opts->bgcolor,
                                   detail_buffer,
                                   sizeof(detail_buffer)) != 0) {
                lsqa_report_invalid_argument(
                    'B',
                    optarg,
                    detail_buffer[0] != '\0'
                        ? detail_buffer
                        : "cannot parse bgcolor option.");
                parse_status = -1;
                goto cleanup;
            }
            opts->has_bgcolor = 1;
            break;
        case 'd':
            if (lsqa_parse_dequantize_method(
                        optarg,
                        &opts->dequantize_method,
                        &opts->dequantize_selective_blur_threshold,
                        detail_buffer,
                        sizeof(detail_buffer)) != 0) {
                lsqa_report_invalid_argument(
                    'd',
                    optarg,
                    detail_buffer[0] != '\0'
                        ? detail_buffer
                        : g_lsqa_dequantize_detail);
                parse_status = -1;
                goto cleanup;
            }
            break;
        case 'S':
            if (lsqa_parse_dequantize_bias(
                        optarg,
                        &opts->dequantize_similarity_bias,
                        detail_buffer,
                        sizeof(detail_buffer),
                        "similarity bias") != 0) {
                lsqa_report_invalid_argument(
                    'S',
                    optarg,
                    detail_buffer[0] != '\0'
                        ? detail_buffer
                        : "similarity bias must be 0-1000.");
                parse_status = -1;
                goto cleanup;
            }
            break;
        case 'e':
            if (lsqa_parse_dequantize_bias(
                        optarg,
                        &opts->dequantize_edge_strength,
                        detail_buffer,
                        sizeof(detail_buffer),
                        "edge bias") != 0) {
                lsqa_report_invalid_argument(
                    'e',
                    optarg,
                    detail_buffer[0] != '\0'
                        ? detail_buffer
                        : "edge bias must be 0-1000.");
                parse_status = -1;
                goto cleanup;
            }
            break;
        case 'b':
        case 'm':
            if (lsqa_parse_selection(optarg, opts, opt == 'b') != 0) {
                parse_status = -1;
                goto cleanup;
            }
            break;
        case '?':
            lsqa_handle_getopt_error(optopt,
                                     (optind > 0 && optind <= scan_argc)
                                         ? scan_argv[optind - 1]
                                         : NULL);
            parse_status = -1;
            goto cleanup;
        default:
            lsqa_report_unrecognized_option(opt, NULL);
            parse_status = -1;
            goto cleanup;
        }
    }

    if (!lsqa_valid_color_spec(opts->ref_spec)
            || !lsqa_valid_color_spec(opts->out_spec)) {
        lsqa_set_parse_error("invalid input colorspace SPEC; see --help");
        parse_status = -1;
        goto cleanup;
    }
    if (strcmp(opts->out_spec, "auto") != 0
            && opts->dequantize_method != SIXEL_DEQUANTIZE_NONE) {
        lsqa_set_parse_error(
            "explicit target colorspace with dequantize is not supported");
        parse_status = -1;
        goto cleanup;
    }

    argi = optind;
    if (scan_argc - argi < 1 || scan_argc - argi > 2) {
        lsqa_set_parse_error("lsqa: invalid number of arguments.");
        parse_status = -1;
        goto cleanup;
    }

    ref_arg = scan_argv[argi];
    opts->ref_path = ref_arg;
    argi += 1;

    if (scan_argc - argi >= 1) {
        out_arg = scan_argv[argi];
        if (strcmp(out_arg, "-") == 0) {
            opts->out_path = "-";
        } else {
            opts->out_path = out_arg;
        }
    }

    if (strcmp(opts->ref_path, "-") == 0
            && strcmp(opts->out_path, "-") == 0) {
        lsqa_set_parse_error("only one image input may use stdin");
        parse_status = -1;
        goto cleanup;
    }
    if (lsqa_apply_env_overrides(opts) != 0) {
        parse_status = -1;
        goto cleanup;
    }
    /* Select just the union needed for output and gates. An unfiltered,
     * ungated invocation retains the complete default report. */
    if (opts->report_count || opts->baseline_count) {
        opts->metric_mask = opts->report_mask | opts->baseline_mask;
    }
    if (lsqa_video_environment(opts) != 0) {
        parse_status = -1;
        goto cleanup;
    }
    if ((!opts->animation_input && opts->video_specified != 0)
            || (opts->error_map[0] != '\0' && opts->frame_report[0] == '\0')
            || (opts->target_timeline[0] != '\0' && opts->reference_fps > 0)
            || (opts->clock_alignment != (opts->display_timeline[0] != '\0'))
            || (opts->clock_alignment && opts->target_timeline[0] != '\0')
            || ((opts->video_specified & 128u) != 0
                && opts->frame_report[0] == '\0')
            || (opts->animation_input
                && (opts->dequantize_method != SIXEL_DEQUANTIZE_NONE
                    || strcmp(opts->ref_spec, "auto") != 0))) {
        lsqa_set_parse_error("video options require -Ma; -E needs -o; "
            "-J and -f conflict; -Aclock needs -C and excludes -J; "
            "-t needs -o; video requires -dnone and -Rauto");
        parse_status = -1;
        goto cleanup;
    }

    /* Match img2sixel while preserving explicit environment and options. */
    if (cli_apply_env_default("SIXEL_LOADER_CMS_ENGINE", "auto") != 0) {
        lsqa_set_parse_error("lsqa: failed to set CMS default.");
        parse_status = -1;
        goto cleanup;
    }

    /* Both image loads must see the same explicit process default. */
    if (cms_choice >= 0
            && cli_apply_env_assignment(g_lsqa_cms_assignments[cms_choice],
                                        detail_buffer,
                                        sizeof(detail_buffer)) != 0) {
        lsqa_set_parse_error(detail_buffer);
        parse_status = -1;
        goto cleanup;
    }

    if (opts->progressive_input &&
            opts->dequantize_method != SIXEL_DEQUANTIZE_NONE) {
        lsqa_set_parse_error("multi-pass input requires dequantize=none");
        parse_status = -1;
        goto cleanup;
    }
    verbose_env = lsqa_getenv_dup("LSQA_VERBOSE");
    if (verbose_env != NULL && verbose_env[0] != '\0') {
        if (lsqa_parse_boolean(verbose_env, &verbose_value) != 0) {
            lsqa_set_parse_error(
                "lsqa: LSQA_VERBOSE expects 0/1 or false/true.");
            parse_status = -1;
            goto cleanup;
        }
        opts->verbose = verbose_value;
    }

    parse_status = 0;

cleanup:
    free(verbose_env);
    free(scan_argv);
    return parse_status;
}

static int
lsqa_run_video(Options const *opts, sixel_allocator_t *allocator)
{
    lsqa_video_options_t video;
    double means[SIXEL_ASSESSMENT_METRIC_COUNT];
    unsigned int valid_mask;
    SIXELSTATUS status;
    Metrics metrics;
    int i;
    int result;

    memset(&video, 0, sizeof(video));
    video.reference = opts->ref_path;
    video.target = opts->out_path;
    video.timeline = opts->target_timeline[0] ? opts->target_timeline : NULL;
    video.report = opts->frame_report[0] ? opts->frame_report : NULL;
    video.display_timeline = opts->display_timeline[0]
        ? opts->display_timeline : NULL;
    video.clock_alignment = opts->clock_alignment;
    video.persistence_time = opts->persistence_time;
    video.error_map = opts->error_map[0] ? opts->error_map : NULL;
    video.loaders = opts->loader_order;
    video.target_colorspace = opts->out_spec;
    video.background = opts->has_bgcolor ? opts->bgcolor : NULL;
    video.metrics = opts->metric_mask;
    video.ppd = opts->scielab_ppd;
    video.threshold = opts->scielab_threshold;
    video.reference_fps = opts->reference_fps;
    video.signal_space = opts->compare_colorspace;
    video.signal_bytes = opts->compare_precision
        == LSQA_COMPARE_PRECISION_REFERENCE ? -1
        : opts->compare_precision == LSQA_COMPARE_PRECISION_8BIT;
    video.grayscale = opts->grayscale_compare;
    video.region_window = opts->region_window;
    video.print_summary = !opts->report_count && !opts->baseline_count;
    status = lsqa_video_run(&video, allocator, means, &valid_mask);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "Video assessment failed: %s (%s)\n",
            sixel_helper_format_error(status),
            sixel_helper_get_additional_message());
        return LSQA_EXIT_RUNTIME_FAILED;
    }
    metrics_init(&metrics);
    for (i = 0; i < SIXEL_ASSESSMENT_METRIC_COUNT; ++i) {
        if ((valid_mask & (1u << i)) != 0) {
            metrics_set_value(&metrics, sixel_assessment_metric_name(i),
                means[i], 1);
        }
    }
    /* Preserve the ungated scalar video's unavailable-result error.
     * Gates must instead diagnose unavailable required metrics as exit 5. */
    if (opts->report_count == 1 && opts->baseline_count == 0
            && (valid_mask & opts->report_mask) == 0u) {
        return LSQA_EXIT_RUNTIME_FAILED;
    }
    result = lsqa_print_selection(opts, &metrics, means);
    if (result != LSQA_EXIT_SUCCESS) {
        return result;
    }
    return lsqa_check_baselines(opts, &metrics);
}

int
main(int argc, char **argv)
{
    Options opts;
    char const *drawing[2];
    sixel_allocator_t *allocator;
    sixel_assessment_t *assessment;
    SIXELSTATUS status;
    sixel_frame_t *ref_frame;
    sixel_frame_t *out_frame;
    Metrics metrics;
    JsonCollector collector;
    int exit_code;
    char const *parse_message;
    int parse_status;
    int reference_colorspace;
    int reference_precision;
    int target_colorspace;
    int target_precision;
    size_t input_passes;

    allocator = NULL;
    assessment = NULL;
    ref_frame = NULL;
    out_frame = NULL;
    exit_code = LSQA_EXIT_RUNTIME_FAILED;
    parse_message = NULL;
    parse_status = 0;
    reference_colorspace = SIXEL_COLORSPACE_GAMMA;
    reference_precision = LSQA_COMPARE_PRECISION_8BIT;
    target_colorspace = SIXEL_COLORSPACE_GAMMA;
    target_precision = LSQA_COMPARE_PRECISION_8BIT;
    input_passes = 0U;

    parse_status = parse_args(argc, argv, &opts);
    if (parse_status != 0) {
        if (parse_status > 0) {
            show_help();
            return LSQA_EXIT_SUCCESS;
        }
        parse_message = sixel_helper_get_additional_message();
        if (parse_message != NULL && parse_message[0] != '\0') {
            fprintf(stderr, "%s\n", parse_message);
        }
        print_usage(argv[0]);
        return LSQA_EXIT_BAD_ARGS;
    }

    status = sixel_allocator_new(&allocator,
                                 malloc,
                                 calloc,
                                 realloc,
                                 free);
    if (SIXEL_FAILED(status) || allocator == NULL) {
        fprintf(stderr,
                "Failed to allocate loader: %s\n",
                sixel_helper_format_error(status));
        return LSQA_EXIT_RUNTIME_FAILED;
    }

    if (opts.animation_input) {
        exit_code = lsqa_run_video(&opts, allocator);
        sixel_allocator_unref(allocator);
        return exit_code;
    }

    if (load_input_frame(opts.ref_path, opts.ref_spec, opts.direct,
            allocator, opts.loader_order,
            opts.has_bgcolor ? opts.bgcolor : NULL, &drawing[0],
            0, NULL, &ref_frame)
            != 0) {
        sixel_allocator_unref(allocator);
        return LSQA_EXIT_LOAD_FAILED;
    }
    if (opts.dequantize_method != SIXEL_DEQUANTIZE_NONE) {
        if (load_dequantized_target_frame(
                    opts.out_path,
                    allocator,
                    opts.dequantize_method,
                    opts.dequantize_similarity_bias,
                    opts.dequantize_edge_strength,
                    opts.dequantize_selective_blur_threshold,
                    opts.direct, opts.has_bgcolor ? opts.bgcolor : NULL,
                    &out_frame) != 0) {
            sixel_frame_unref(ref_frame);
            sixel_allocator_unref(allocator);
            return LSQA_EXIT_LOAD_FAILED;
        }
        drawing[1] = opts.direct ? "direct" : "indexed";
    } else if (load_input_frame(opts.out_path, opts.out_spec, opts.direct,
            allocator, opts.loader_order,
            opts.has_bgcolor ? opts.bgcolor : NULL, &drawing[1],
            opts.progressive_input, &input_passes, &out_frame)
            != 0) {
        sixel_frame_unref(ref_frame);
        sixel_allocator_unref(allocator);
        return LSQA_EXIT_LOAD_FAILED;
    }

    if (opts.verbose && opts.progressive_input) {
        fprintf(stderr, "input-mode: 6delta-progressive, passes: %zu\n",
                input_passes);
    }
    reference_colorspace = sixel_frame_get_colorspace(ref_frame);
    reference_precision = lsqa_precision_from_pixelformat(
        sixel_frame_get_pixelformat(ref_frame));
    if (opts.compare_colorspace == LSQA_COMPARE_COLORSPACE_REFERENCE) {
        target_colorspace = reference_colorspace;
    } else {
        target_colorspace = opts.compare_colorspace;
    }
    if (opts.compare_precision == LSQA_COMPARE_PRECISION_REFERENCE) {
        target_precision = reference_precision;
    } else {
        target_precision = opts.compare_precision;
    }

    /* Preserve legacy post-loader alpha handling for auto inputs. Explicit
     * SIXEL frames have already selected the independent sRGB background. */
    status = sixel_frame_strip_alpha(ref_frame,
                                      opts.has_bgcolor ? opts.bgcolor : NULL);
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_frame_strip_alpha(out_frame,
                    opts.has_bgcolor ? opts.bgcolor : NULL);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_assessment_new(&assessment, allocator);
    }
    if (SIXEL_FAILED(status) || assessment == NULL) {
        fprintf(stderr,
                "Failed to create assessment object: %s\n",
                sixel_helper_format_error(status));
        sixel_frame_unref(ref_frame);
        sixel_frame_unref(out_frame);
        sixel_allocator_unref(allocator);
        return LSQA_EXIT_RUNTIME_FAILED;
    }

    assessment->signal_space = target_colorspace;
    assessment->signal_bytes = target_precision == LSQA_COMPARE_PRECISION_8BIT;
    assessment->grayscale = opts.grayscale_compare;
    assessment->scielab_ppd = opts.scielab_ppd;
    assessment->region_window = opts.region_window;
    assessment->direct = opts.direct;
    assessment->drawing[0] = drawing[0];
    assessment->drawing[1] = drawing[1];
    assessment->color_spec[0] = opts.ref_spec;
    assessment->color_spec[1] = opts.out_spec;
    assessment->color_origin[0] = strcmp(opts.ref_spec, "auto") == 0
        ? (strcmp(drawing[0], "direct") == 0
           ? "original-sixel-srgb" : "legacy-loader-managed")
        : "explicit-original-sixel";
    assessment->color_origin[1] = strcmp(opts.out_spec, "auto") == 0
        ? (strcmp(drawing[1], "direct") == 0
           ? "original-sixel-srgb" : "legacy-loader-managed")
        : "explicit-original-sixel";
    memcpy(assessment->background, opts.bgcolor, 3);

    sixel_assessment_select_sections(assessment,
                                     SIXEL_ASSESSMENT_SECTION_QUALITY);

    sixel_assessment_select_metrics(assessment, opts.metric_mask);

    status = sixel_assessment_analyze(assessment, ref_frame, out_frame);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "Assessment failed: %s\n",
                sixel_helper_format_error(status));
        sixel_assessment_unref(assessment);
        sixel_frame_unref(ref_frame);
        sixel_frame_unref(out_frame);
        sixel_allocator_unref(allocator);
        return LSQA_EXIT_RUNTIME_FAILED;
    }

    metrics_init(&metrics);
    collector.metrics = &metrics;

    if (opts.report_count || opts.baseline_count) {
        collector.stream = NULL;
        status = sixel_assessment_get_json(assessment,
                                           SIXEL_ASSESSMENT_SECTION_QUALITY,
                                           json_collect_callback,
                                           &collector);
        if (SIXEL_FAILED(status)) {
            fprintf(stderr,
                    "Failed to emit JSON: %s\n",
                    sixel_helper_format_error(status));
            sixel_assessment_unref(assessment);
            sixel_frame_unref(ref_frame);
            sixel_frame_unref(out_frame);
            sixel_allocator_unref(allocator);
            return LSQA_EXIT_RUNTIME_FAILED;
        }

        exit_code = lsqa_print_selection(&opts, &metrics, NULL);
        if (opts.verbose) {
            verbose_print(&metrics);
        }
        if (exit_code == LSQA_EXIT_SUCCESS) {
            exit_code = lsqa_check_baselines(&opts, &metrics);
        }
        sixel_assessment_unref(assessment);
        sixel_frame_unref(ref_frame);
        sixel_frame_unref(out_frame);
        sixel_allocator_unref(allocator);
        return exit_code;
    }

    collector.stream = stdout;
    status = sixel_assessment_get_json(assessment,
                                       SIXEL_ASSESSMENT_SECTION_QUALITY,
                                       json_collect_callback,
                                       &collector);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "Failed to emit JSON: %s\n",
                sixel_helper_format_error(status));
        sixel_assessment_unref(assessment);
        sixel_frame_unref(ref_frame);
        sixel_frame_unref(out_frame);
        sixel_allocator_unref(allocator);
        return LSQA_EXIT_RUNTIME_FAILED;
    }

    if (opts.verbose) {
        verbose_print(&metrics);
    }

    exit_code = LSQA_EXIT_SUCCESS;

    sixel_assessment_unref(assessment);
    sixel_frame_unref(ref_frame);
    sixel_frame_unref(out_frame);
    sixel_allocator_unref(allocator);
    return exit_code;
}
