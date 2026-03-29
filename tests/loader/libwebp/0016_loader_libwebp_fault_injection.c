/*
 * Verify libwebp loader reports decoder setup failures via fault injection.
 */

#include <string.h>

#include "tests/loader/pixelformat_test_common.h"

#if HAVE_WEBP
#include <webp/decode.h>
#include <webp/demux.h>
#include <webp/mux.h>

typedef enum webp_fi_failpoint {
    WEBP_FI_FAIL_NONE = 0,
    WEBP_FI_FAIL_DEMUX,
    WEBP_FI_FAIL_OPTIONS_INIT,
    WEBP_FI_FAIL_DECODER_NEW,
    WEBP_FI_FAIL_DECODER_GET_INFO,
    WEBP_FI_FAIL_DECODER_GET_NEXT,
    WEBP_FI_FAIL_STATIC_RGB_INTO,
    WEBP_FI_FAIL_STATIC_RGBA_INTO
} webp_fi_failpoint_t;

static webp_fi_failpoint_t g_webp_fi_failpoint = WEBP_FI_FAIL_NONE;

static int (*g_real_WebPAnimDecoderOptionsInit)(WebPAnimDecoderOptions *) =
    WebPAnimDecoderOptionsInit;
static WebPAnimDecoder *(*g_real_WebPAnimDecoderNew)(WebPData const *,
                                                     WebPAnimDecoderOptions const *) =
    WebPAnimDecoderNew;
static int (*g_real_WebPAnimDecoderGetInfo)(WebPAnimDecoder const *,
                                            WebPAnimInfo *) =
    WebPAnimDecoderGetInfo;
static int (*g_real_WebPAnimDecoderGetNext)(WebPAnimDecoder *,
                                            uint8_t **,
                                            int *) =
    WebPAnimDecoderGetNext;
static uint8_t *(*g_real_WebPDecodeRGBInto)(uint8_t const *,
                                            size_t,
                                            uint8_t *,
                                            size_t,
                                            int) =
    WebPDecodeRGBInto;
static uint8_t *(*g_real_WebPDecodeRGBAInto)(uint8_t const *,
                                             size_t,
                                             uint8_t *,
                                             size_t,
                                             int) =
    WebPDecodeRGBAInto;

static int
webpfi_WebPAnimDecoderOptionsInit(WebPAnimDecoderOptions *options)
{
    if (g_webp_fi_failpoint == WEBP_FI_FAIL_OPTIONS_INIT) {
        return 0;
    }
    return g_real_WebPAnimDecoderOptionsInit(options);
}

static WebPAnimDecoder *
webpfi_WebPAnimDecoderNew(WebPData const *webp_data,
                          WebPAnimDecoderOptions const *decoder_options)
{
    if (g_webp_fi_failpoint == WEBP_FI_FAIL_DECODER_NEW) {
        return NULL;
    }
    return g_real_WebPAnimDecoderNew(webp_data, decoder_options);
}

static int
webpfi_WebPAnimDecoderGetInfo(WebPAnimDecoder const *decoder,
                              WebPAnimInfo *anim_info)
{
    if (g_webp_fi_failpoint == WEBP_FI_FAIL_DECODER_GET_INFO) {
        return 0;
    }
    return g_real_WebPAnimDecoderGetInfo(decoder, anim_info);
}

static int
webpfi_WebPAnimDecoderGetNext(WebPAnimDecoder *decoder,
                              uint8_t **buf_ptr,
                              int *timestamp)
{
    if (g_webp_fi_failpoint == WEBP_FI_FAIL_DECODER_GET_NEXT) {
        return 0;
    }
    return g_real_WebPAnimDecoderGetNext(decoder, buf_ptr, timestamp);
}

static uint8_t *
webpfi_WebPDecodeRGBInto(uint8_t const *data,
                         size_t data_size,
                         uint8_t *output_buffer,
                         size_t output_buffer_size,
                         int output_stride)
{
    if (g_webp_fi_failpoint == WEBP_FI_FAIL_STATIC_RGB_INTO) {
        return NULL;
    }
    return g_real_WebPDecodeRGBInto(data,
                                    data_size,
                                    output_buffer,
                                    output_buffer_size,
                                    output_stride);
}

static uint8_t *
webpfi_WebPDecodeRGBAInto(uint8_t const *data,
                          size_t data_size,
                          uint8_t *output_buffer,
                          size_t output_buffer_size,
                          int output_stride)
{
    if (g_webp_fi_failpoint == WEBP_FI_FAIL_STATIC_RGBA_INTO) {
        return NULL;
    }
    return g_real_WebPDecodeRGBAInto(data,
                                     data_size,
                                     output_buffer,
                                     output_buffer_size,
                                     output_stride);
}

static WebPDemuxer *
webpfi_WebPDemux(WebPData const *webp_data)
{
    if (g_webp_fi_failpoint == WEBP_FI_FAIL_DEMUX) {
        return NULL;
    }
    return WebPDemuxInternal(webp_data, 0, NULL, WEBP_DEMUX_ABI_VERSION);
}

#define WebPAnimDecoderOptionsInit webpfi_WebPAnimDecoderOptionsInit
#define WebPAnimDecoderNew webpfi_WebPAnimDecoderNew
#define WebPAnimDecoderGetInfo webpfi_WebPAnimDecoderGetInfo
#define WebPAnimDecoderGetNext webpfi_WebPAnimDecoderGetNext
#define WebPDecodeRGBInto webpfi_WebPDecodeRGBInto
#define WebPDecodeRGBAInto webpfi_WebPDecodeRGBAInto
#define WebPDemux webpfi_WebPDemux

/*
 * Avoid global symbol collisions with the linked libsixel objects while
 * keeping access to load_with_libwebp() inside this translation unit.
 */
#define sixel_loader_libwebp_ref testonly_loader_libwebp_ref
#define sixel_loader_libwebp_unref testonly_loader_libwebp_unref
#define sixel_loader_libwebp_setopt testonly_loader_libwebp_setopt
#define sixel_loader_libwebp_load testonly_loader_libwebp_load
#define sixel_loader_libwebp_name testonly_loader_libwebp_name
#define sixel_loader_libwebp_new testonly_loader_libwebp_new
#define sixel_loader_libwebp_placeholder_function \
    testonly_loader_libwebp_placeholder_function

#include "src/loader-libwebp.c"

#undef sixel_loader_libwebp_ref
#undef sixel_loader_libwebp_unref
#undef sixel_loader_libwebp_setopt
#undef sixel_loader_libwebp_load
#undef sixel_loader_libwebp_name
#undef sixel_loader_libwebp_new
#undef sixel_loader_libwebp_placeholder_function

#undef WebPAnimDecoderOptionsInit
#undef WebPAnimDecoderNew
#undef WebPAnimDecoderGetInfo
#undef WebPAnimDecoderGetNext
#undef WebPDecodeRGBInto
#undef WebPDecodeRGBAInto
#undef WebPDemux

static SIXELSTATUS
capture_noop_frame(sixel_frame_t *frame, void *data)
{
    (void)frame;
    (void)data;
    return SIXEL_OK;
}

static int
run_decoder_setup_fail_case(webp_fi_failpoint_t failpoint,
                            char const *expected_message,
                            char const *label)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_chunk_t *chunk;
    char const *source_root;
    char image_path[PATH_MAX];
    int cancel_flag;
    char const *message;
    int result;

    status = SIXEL_FALSE;
    allocator = NULL;
    chunk = NULL;
    source_root = NULL;
    cancel_flag = 0;
    message = NULL;
    result = 1;

    source_root = getenv("MESON_SOURCE_ROOT");
    if (source_root == NULL) {
        source_root = getenv("abs_top_srcdir");
    }
    if (source_root == NULL) {
        source_root = getenv("TOP_SRCDIR");
    }
    if (source_root == NULL) {
        source_root = ".";
    }

    if (build_image_path(source_root,
                         "/tests/data/inputs/formats/animated-lossless-8x8-2frame-min.webp",
                         image_path,
                         sizeof(image_path)) != 0) {
        fprintf(stderr, "%s: failed to build image path\n", label);
        return 1;
    }

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: allocator init failed\n", label);
        return 1;
    }

    status = sixel_chunk_new(&chunk, image_path, 0, &cancel_flag, allocator);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: failed to read input chunk\n", label);
        goto cleanup;
    }

    sixel_helper_set_additional_message(NULL);
    g_webp_fi_failpoint = failpoint;
    status = load_with_libwebp(chunk,
                               0,
                               0,
                               0,
                               256,
                               NULL,
                               SIXEL_LOOP_DISABLE,
                               0,
                               0,
                               capture_noop_frame,
                               NULL);
    g_webp_fi_failpoint = WEBP_FI_FAIL_NONE;

    if (status != SIXEL_WEBP_ERROR) {
        fprintf(stderr,
                "%s: expected SIXEL_WEBP_ERROR, got %d\n",
                label,
                (int)status);
        goto cleanup;
    }

    message = sixel_helper_get_additional_message();
    if (message == NULL || strstr(message, expected_message) == NULL) {
        fprintf(stderr,
                "%s: missing diagnostic '%s' (actual='%s')\n",
                label,
                expected_message,
                message != NULL ? message : "(null)");
        goto cleanup;
    }

    result = 0;

cleanup:
    sixel_chunk_destroy(chunk);
    sixel_allocator_unref(allocator);
    g_webp_fi_failpoint = WEBP_FI_FAIL_NONE;
    return result;
}

static int
run_demux_fail_case(char const *label)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_chunk_t *chunk;
    char const *source_root;
    char image_path[PATH_MAX];
    int cancel_flag;
    char const *message;
    int result;

    status = SIXEL_FALSE;
    allocator = NULL;
    chunk = NULL;
    source_root = NULL;
    cancel_flag = 0;
    message = NULL;
    result = 1;

    source_root = getenv("MESON_SOURCE_ROOT");
    if (source_root == NULL) {
        source_root = getenv("abs_top_srcdir");
    }
    if (source_root == NULL) {
        source_root = getenv("TOP_SRCDIR");
    }
    if (source_root == NULL) {
        source_root = ".";
    }

    if (build_image_path(source_root,
                         WEBP_IMAGE_PATH,
                         image_path,
                         sizeof(image_path)) != 0) {
        fprintf(stderr, "%s: failed to build image path\n", label);
        return 1;
    }

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: allocator init failed\n", label);
        return 1;
    }

    status = sixel_chunk_new(&chunk, image_path, 0, &cancel_flag, allocator);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: failed to read input chunk\n", label);
        goto cleanup;
    }

    sixel_helper_set_additional_message(NULL);
    g_webp_fi_failpoint = WEBP_FI_FAIL_DEMUX;
    status = load_with_libwebp(chunk,
                               1,
                               0,
                               0,
                               256,
                               NULL,
                               SIXEL_LOOP_DISABLE,
                               0,
                               0,
                               capture_noop_frame,
                               NULL);
    g_webp_fi_failpoint = WEBP_FI_FAIL_NONE;

    if (status != SIXEL_BAD_INPUT) {
        fprintf(stderr,
                "%s: expected SIXEL_BAD_INPUT, got %d\n",
                label,
                (int)status);
        goto cleanup;
    }

    message = sixel_helper_get_additional_message();
    if (message == NULL ||
        strstr(message, "load_with_libwebp: WebPDemux failed.") == NULL) {
        fprintf(stderr,
                "%s: missing demux diagnostic (actual='%s')\n",
                label,
                message != NULL ? message : "(null)");
        goto cleanup;
    }

    result = 0;

cleanup:
    sixel_chunk_destroy(chunk);
    sixel_allocator_unref(allocator);
    g_webp_fi_failpoint = WEBP_FI_FAIL_NONE;
    return result;
}

static int
run_static_decode_fail_case(webp_fi_failpoint_t failpoint,
                            char const *image_relative_path,
                            char const *force_rgb_env_value,
                            char const *expected_message,
                            char const *label)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_chunk_t *chunk;
    char const *source_root;
    char image_path[PATH_MAX];
    int cancel_flag;
    unsigned char *pixels;
    int width;
    int height;
    int pixelformat;
    int cms_converted;
    char const *message;
    int result;

    status = SIXEL_FALSE;
    allocator = NULL;
    chunk = NULL;
    source_root = NULL;
    cancel_flag = 0;
    pixels = NULL;
    width = 0;
    height = 0;
    pixelformat = 0;
    cms_converted = 0;
    message = NULL;
    result = 1;

    source_root = getenv("MESON_SOURCE_ROOT");
    if (source_root == NULL) {
        source_root = getenv("abs_top_srcdir");
    }
    if (source_root == NULL) {
        source_root = getenv("TOP_SRCDIR");
    }
    if (source_root == NULL) {
        source_root = ".";
    }

    if (build_image_path(source_root,
                         image_relative_path,
                         image_path,
                         sizeof(image_path)) != 0) {
        fprintf(stderr, "%s: failed to build image path\n", label);
        return 1;
    }

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: allocator init failed\n", label);
        return 1;
    }

    status = sixel_chunk_new(&chunk, image_path, 0, &cancel_flag, allocator);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: failed to read input chunk\n", label);
        goto cleanup;
    }

    if (force_rgb_env_value == NULL) {
        force_rgb_env_value = "0";
    }
    if (sixel_compat_setenv("SIXEL_LOADER_LIBWEBP_LOSSY_USE_RGB_DECODE",
                            force_rgb_env_value) != 0) {
        fprintf(stderr, "%s: failed to configure force-rgb env\n", label);
        goto cleanup;
    }

    sixel_helper_set_additional_message(NULL);
    g_webp_fi_failpoint = failpoint;
    status = load_webp(&pixels,
                       chunk->buffer,
                       chunk->size,
                       &width,
                       &height,
                       &pixelformat,
                       0,
                       NULL,
                       0u,
                       &cms_converted,
                       NULL,
                       allocator,
                       NULL);
    g_webp_fi_failpoint = WEBP_FI_FAIL_NONE;

    if (status != SIXEL_BAD_INPUT) {
        fprintf(stderr,
                "%s: expected SIXEL_BAD_INPUT, got %d\n",
                label,
                (int)status);
        goto cleanup;
    }

    message = sixel_helper_get_additional_message();
    if (message == NULL || strstr(message, expected_message) == NULL) {
        fprintf(stderr,
                "%s: missing diagnostic '%s' (actual='%s')\n",
                label,
                expected_message,
                message != NULL ? message : "(null)");
        goto cleanup;
    }

    result = 0;

cleanup:
    (void)sixel_compat_setenv("SIXEL_LOADER_LIBWEBP_LOSSY_USE_RGB_DECODE",
                              "0");
    g_webp_fi_failpoint = WEBP_FI_FAIL_NONE;
    if (pixels != NULL) {
        sixel_allocator_free(allocator, pixels);
    }
    sixel_chunk_destroy(chunk);
    sixel_allocator_unref(allocator);
    return result;
}

static int
run_libwebp_fault_injection_test(void)
{
    int status;

    status = run_demux_fail_case("libwebp fault injection demux");
    if (status != 0) {
        return status;
    }

    status = run_decoder_setup_fail_case(
        WEBP_FI_FAIL_OPTIONS_INIT,
        "load_with_libwebp: WebPAnimDecoderOptionsInit failed.",
        "libwebp fault injection options-init");
    if (status != 0) {
        return status;
    }

    status = run_decoder_setup_fail_case(
        WEBP_FI_FAIL_DECODER_NEW,
        "load_with_libwebp: WebPAnimDecoderNew failed.",
        "libwebp fault injection decoder-new");
    if (status != 0) {
        return status;
    }

    status = run_decoder_setup_fail_case(
        WEBP_FI_FAIL_DECODER_GET_INFO,
        "load_with_libwebp: WebPAnimDecoderGetInfo failed.",
        "libwebp fault injection decoder-getinfo");
    if (status != 0) {
        return status;
    }

    status = run_decoder_setup_fail_case(
        WEBP_FI_FAIL_DECODER_GET_NEXT,
        "load_with_libwebp: WebPAnimDecoderGetNext failed.",
        "libwebp fault injection decoder-getnext");
    if (status != 0) {
        return status;
    }

    status = run_static_decode_fail_case(
        WEBP_FI_FAIL_STATIC_RGB_INTO,
        WEBP_IMAGE_PATH,
        "1",
        "load_webp: WebPDecodeRGBInto failed.",
        "libwebp fault injection static-rgbinto");
    if (status != 0) {
        return status;
    }

    return run_static_decode_fail_case(
        WEBP_FI_FAIL_STATIC_RGBA_INTO,
        "/tests/data/inputs/formats/webp-static-alpha-keycolor-lossy.webp",
        "0",
        "load_webp: WebPDecodeRGBAInto failed.",
        "libwebp fault injection static-rgbainto");
}
#endif

int
test_loader_0024_loader_libwebp_fault_injection(int argc, char **argv)
{
    (void)argc;
    (void)argv;

#if HAVE_WEBP
    return run_libwebp_fault_injection_test();
#else
    fprintf(stderr, "libwebp loader unavailable\n");
    return SIXEL_TEST_SKIP;
#endif
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
