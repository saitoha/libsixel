/*
 * Verify libwebp loader diagnostics for fault-injection paths.
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
    WEBP_FI_FAIL_GET_FEATURES,
    WEBP_FI_FAIL_OPTIONS_INIT,
    WEBP_FI_FAIL_DECODER_NEW,
    WEBP_FI_FAIL_DECODER_GET_INFO,
    WEBP_FI_FAIL_DECODER_FRAME_COUNT_LIMIT,
    WEBP_FI_FAIL_DECODER_HAS_MORE_FRAMES,
    WEBP_FI_FAIL_DECODER_GET_NEXT,
    WEBP_FI_FAIL_LOSSY_INIT_CONFIG,
    WEBP_FI_FAIL_LOSSY_DECODE,
    WEBP_FI_FAIL_LOSSY_YUV_PLANE_MISSING,
    WEBP_FI_FAIL_LOSSY_YUV_STRIDE_INVALID,
    WEBP_FI_FAIL_LOSSY_DIMENSION_MISMATCH,
    WEBP_FI_FAIL_LOSSY_ALLOC,
    WEBP_FI_FAIL_STATIC_RGB_INTO,
    WEBP_FI_FAIL_STATIC_RGBA_INTO,
    WEBP_FI_FAIL_STATIC_ALLOC,
    WEBP_FI_FAIL_ANIMATION_CANVAS_ALLOC,
    WEBP_FI_FAIL_ANMF_EXTRACT_ALLOC
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
static int (*g_real_WebPAnimDecoderHasMoreFrames)(WebPAnimDecoder const *) =
    WebPAnimDecoderHasMoreFrames;
static int (*g_real_WebPAnimDecoderGetNext)(WebPAnimDecoder *,
                                            uint8_t **,
                                            int *) =
    WebPAnimDecoderGetNext;
static int (*g_real_WebPInitDecoderConfig)(WebPDecoderConfig *) =
    WebPInitDecoderConfig;
static VP8StatusCode (*g_real_WebPGetFeatures)(uint8_t const *,
                                               size_t,
                                               WebPBitstreamFeatures *) =
    WebPGetFeatures;
static VP8StatusCode (*g_real_WebPDecode)(uint8_t const *,
                                          size_t,
                                          WebPDecoderConfig *) =
    WebPDecode;
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
static void *(*g_real_sixel_allocator_malloc)(sixel_allocator_t *,
                                              size_t) =
    sixel_allocator_malloc;

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
    int status;

    status = 0;

    if (g_webp_fi_failpoint == WEBP_FI_FAIL_DECODER_GET_INFO) {
        return 0;
    }
    status = g_real_WebPAnimDecoderGetInfo(decoder, anim_info);
    if (status != 0 &&
        g_webp_fi_failpoint == WEBP_FI_FAIL_DECODER_FRAME_COUNT_LIMIT &&
        anim_info != NULL) {
        anim_info->frame_count = 65536u;
    }
    return status;
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

static int
webpfi_WebPAnimDecoderHasMoreFrames(WebPAnimDecoder const *decoder)
{
    if (g_webp_fi_failpoint == WEBP_FI_FAIL_DECODER_HAS_MORE_FRAMES) {
        return 0;
    }
    return g_real_WebPAnimDecoderHasMoreFrames(decoder);
}

static int
webpfi_WebPInitDecoderConfig(WebPDecoderConfig *config)
{
    if (g_webp_fi_failpoint == WEBP_FI_FAIL_LOSSY_INIT_CONFIG) {
        return 0;
    }
    return g_real_WebPInitDecoderConfig(config);
}

static VP8StatusCode
webpfi_WebPGetFeatures(uint8_t const *data,
                       size_t data_size,
                       WebPBitstreamFeatures *features)
{
    if (g_webp_fi_failpoint == WEBP_FI_FAIL_GET_FEATURES) {
        return VP8_STATUS_BITSTREAM_ERROR;
    }
    return g_real_WebPGetFeatures(data, data_size, features);
}

static VP8StatusCode
webpfi_WebPDecode(uint8_t const *data,
                  size_t data_size,
                  WebPDecoderConfig *config)
{
    VP8StatusCode status;

    status = VP8_STATUS_OK;
    if (g_webp_fi_failpoint == WEBP_FI_FAIL_LOSSY_DECODE) {
        return VP8_STATUS_BITSTREAM_ERROR;
    }

    status = g_real_WebPDecode(data, data_size, config);
    if (status != VP8_STATUS_OK) {
        return status;
    }
    if (config == NULL) {
        return status;
    }

    switch (g_webp_fi_failpoint) {
    case WEBP_FI_FAIL_LOSSY_YUV_PLANE_MISSING:
        config->output.u.YUVA.y = NULL;
        break;
    case WEBP_FI_FAIL_LOSSY_YUV_STRIDE_INVALID:
        config->output.u.YUVA.y_stride = 0;
        break;
    case WEBP_FI_FAIL_LOSSY_DIMENSION_MISMATCH:
        config->output.width += 1;
        break;
    default:
        break;
    }

    return status;
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

static void *
webpfi_sixel_allocator_malloc(sixel_allocator_t *allocator, size_t nbytes)
{
    switch (g_webp_fi_failpoint) {
    case WEBP_FI_FAIL_LOSSY_ALLOC:
    case WEBP_FI_FAIL_STATIC_ALLOC:
    case WEBP_FI_FAIL_ANIMATION_CANVAS_ALLOC:
    case WEBP_FI_FAIL_ANMF_EXTRACT_ALLOC:
        return NULL;
    default:
        break;
    }
    return g_real_sixel_allocator_malloc(allocator, nbytes);
}

#define WebPAnimDecoderOptionsInit webpfi_WebPAnimDecoderOptionsInit
#define WebPAnimDecoderNew webpfi_WebPAnimDecoderNew
#define WebPAnimDecoderGetInfo webpfi_WebPAnimDecoderGetInfo
#define WebPAnimDecoderHasMoreFrames webpfi_WebPAnimDecoderHasMoreFrames
#define WebPAnimDecoderGetNext webpfi_WebPAnimDecoderGetNext
#define WebPInitDecoderConfig webpfi_WebPInitDecoderConfig
#define WebPGetFeatures webpfi_WebPGetFeatures
#define WebPDecode webpfi_WebPDecode
#define WebPDecodeRGBInto webpfi_WebPDecodeRGBInto
#define WebPDecodeRGBAInto webpfi_WebPDecodeRGBAInto
#define WebPDemux webpfi_WebPDemux
#define sixel_allocator_malloc webpfi_sixel_allocator_malloc

#define WEBP_FI_MAX_ANIMATION_FRAMES 1024
#define WEBP_MAX_ANIMATION_FRAMES WEBP_FI_MAX_ANIMATION_FRAMES

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
#undef WebPAnimDecoderHasMoreFrames
#undef WebPAnimDecoderGetNext
#undef WebPInitDecoderConfig
#undef WebPGetFeatures
#undef WebPDecode
#undef WebPDecodeRGBInto
#undef WebPDecodeRGBAInto
#undef WebPDemux
#undef sixel_allocator_malloc

#undef WEBP_MAX_ANIMATION_FRAMES

typedef struct webp_lossy_decode_fault_case {
    webp_fi_failpoint_t failpoint;
    SIXELSTATUS expected_status;
    char const *expected_message;
    char const *label;
} webp_lossy_decode_fault_case_t;

static webp_lossy_decode_fault_case_t const g_lossy_decode_fault_cases[] = {
    { WEBP_FI_FAIL_LOSSY_INIT_CONFIG,
      SIXEL_WEBP_ERROR,
      "webp_decode_lossy_to_float32: WebPInitDecoderConfig failed.",
      "libwebp fault injection lossy-init-config" },
    { WEBP_FI_FAIL_LOSSY_DECODE,
      SIXEL_WEBP_ERROR,
      "webp_decode_lossy_to_float32: WebPDecode failed",
      "libwebp fault injection lossy-decode" },
    { WEBP_FI_FAIL_LOSSY_YUV_PLANE_MISSING,
      SIXEL_BAD_INPUT,
      "webp_decode_lossy_to_float32: YUV plane is missing.",
      "libwebp fault injection lossy-yuv-plane-missing" },
    { WEBP_FI_FAIL_LOSSY_YUV_STRIDE_INVALID,
      SIXEL_BAD_INPUT,
      "webp_decode_lossy_to_float32: YUV plane stride is invalid.",
      "libwebp fault injection lossy-yuv-stride-invalid" },
    { WEBP_FI_FAIL_LOSSY_DIMENSION_MISMATCH,
      SIXEL_BAD_INPUT,
      "webp_decode_lossy_to_float32: decoded dimensions mismatch.",
      "libwebp fault injection lossy-dimensions-mismatch" }
};

static SIXELSTATUS
capture_noop_frame(sixel_frame_t *frame, void *data)
{
    (void)frame;
    (void)data;
    return SIXEL_OK;
}

static int
webpfi_build_source_path(char const *relative,
                         char *path,
                         size_t path_size,
                         char const *label)
{
    char const *source_root;

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

    if (build_image_path(source_root, relative, path, path_size) != 0) {
        fprintf(stderr, "%s: failed to build image path\n", label);
        return 1;
    }
    return 0;
}

static int
run_animation_decode_fail_case(webp_fi_failpoint_t failpoint,
                               SIXELSTATUS expected_status,
                               int fstatic,
                               char const *expected_message,
                               char const *label)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_chunk_t *chunk;
    char image_path[PATH_MAX];
    int cancel_flag;
    char const *message;
    int result;

    status = SIXEL_FALSE;
    allocator = NULL;
    chunk = NULL;
    cancel_flag = 0;
    message = NULL;
    result = 1;

    if (webpfi_build_source_path(
            "/tests/data/inputs/formats/animated-lossless-8x8-2frame-min.webp",
            image_path,
            sizeof(image_path),
            label) != 0) {
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
                               fstatic,
                               0,
                               256,
                               NULL,
                               SIXEL_LOOP_DISABLE,
                               0,
                               0,
                               capture_noop_frame,
                               NULL);
    g_webp_fi_failpoint = WEBP_FI_FAIL_NONE;

    if (status != expected_status) {
        fprintf(stderr,
                "%s: expected %d, got %d\n",
                label,
                (int)expected_status,
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
    char image_path[PATH_MAX];
    int cancel_flag;
    char const *message;
    int result;

    status = SIXEL_FALSE;
    allocator = NULL;
    chunk = NULL;
    cancel_flag = 0;
    message = NULL;
    result = 1;

    if (webpfi_build_source_path(WEBP_IMAGE_PATH,
                                 image_path,
                                 sizeof(image_path),
                                 label) != 0) {
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
run_load_webp_fail_case(webp_fi_failpoint_t failpoint,
                        char const *image_relative_path,
                        char const *force_rgb_env_value,
                        SIXELSTATUS expected_status,
                        char const *expected_message,
                        char const *label)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_chunk_t *chunk;
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
    cancel_flag = 0;
    pixels = NULL;
    width = 0;
    height = 0;
    pixelformat = 0;
    cms_converted = 0;
    message = NULL;
    result = 1;

    if (webpfi_build_source_path(image_relative_path,
                                 image_path,
                                 sizeof(image_path),
                                 label) != 0) {
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

    if (status != expected_status) {
        fprintf(stderr,
                "%s: expected %d, got %d\n",
                label,
                (int)expected_status,
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
run_fault_case_from_spec(webp_lossy_decode_fault_case_t const *spec)
{
    if (spec == NULL) {
        return 1;
    }
    return run_load_webp_fail_case(spec->failpoint,
                                   WEBP_IMAGE_PATH,
                                   "0",
                                   spec->expected_status,
                                   spec->expected_message,
                                   spec->label);
}

static int
run_lossy_fault_case(webp_fi_failpoint_t failpoint)
{
    size_t index;
    webp_lossy_decode_fault_case_t const *spec;

    index = 0u;
    spec = NULL;

    for (index = 0u; index < (sizeof(g_lossy_decode_fault_cases) /
                              sizeof(g_lossy_decode_fault_cases[0])); index++) {
        if (g_lossy_decode_fault_cases[index].failpoint == failpoint) {
            spec = &g_lossy_decode_fault_cases[index];
            break;
        }
    }

    return run_fault_case_from_spec(spec);
}

static int
run_fault_options_init_case(void)
{
    return run_animation_decode_fail_case(
        WEBP_FI_FAIL_OPTIONS_INIT,
        SIXEL_WEBP_ERROR,
        0,
        "load_with_libwebp: WebPAnimDecoderOptionsInit failed.",
        "libwebp fault injection options-init");
}

static int
run_fault_decoder_new_case(void)
{
    return run_animation_decode_fail_case(
        WEBP_FI_FAIL_DECODER_NEW,
        SIXEL_WEBP_ERROR,
        0,
        "load_with_libwebp: WebPAnimDecoderNew failed.",
        "libwebp fault injection decoder-new");
}

static int
run_fault_decoder_getinfo_case(void)
{
    return run_animation_decode_fail_case(
        WEBP_FI_FAIL_DECODER_GET_INFO,
        SIXEL_WEBP_ERROR,
        0,
        "load_with_libwebp: WebPAnimDecoderGetInfo failed.",
        "libwebp fault injection decoder-getinfo");
}

static int
run_fault_decoder_getnext_case(void)
{
    return run_animation_decode_fail_case(
        WEBP_FI_FAIL_DECODER_GET_NEXT,
        SIXEL_WEBP_ERROR,
        0,
        "load_with_libwebp: WebPAnimDecoderGetNext failed.",
        "libwebp fault injection decoder-getnext");
}

static int
run_fault_decoder_frame_count_limit_case(void)
{
    return run_animation_decode_fail_case(
        WEBP_FI_FAIL_DECODER_FRAME_COUNT_LIMIT,
        SIXEL_BAD_INPUT,
        0,
        "load_with_libwebp: animation frame count exceeds limit.",
        "libwebp fault injection decoder-frame-count-limit");
}

static int
run_fault_get_features_animation_case(void)
{
    return run_animation_decode_fail_case(
        WEBP_FI_FAIL_GET_FEATURES,
        SIXEL_BAD_INPUT,
        0,
        "load_with_libwebp: WebPGetFeatures failed",
        "libwebp fault injection animated-get-features");
}

static int
run_fault_static_rgbinto_case(void)
{
    return run_load_webp_fail_case(
        WEBP_FI_FAIL_STATIC_RGB_INTO,
        WEBP_IMAGE_PATH,
        "1",
        SIXEL_BAD_INPUT,
        "load_webp: WebPDecodeRGBInto failed.",
        "libwebp fault injection static-rgbinto");
}

static int
run_fault_static_rgbainto_case(void)
{
    return run_load_webp_fail_case(
        WEBP_FI_FAIL_STATIC_RGBA_INTO,
        "/tests/data/inputs/formats/webp-static-alpha-keycolor-lossy.webp",
        "0",
        SIXEL_BAD_INPUT,
        "load_webp: WebPDecodeRGBAInto failed.",
        "libwebp fault injection static-rgbainto");
}

static int
run_fault_get_features_static_case(void)
{
    return run_load_webp_fail_case(
        WEBP_FI_FAIL_GET_FEATURES,
        WEBP_IMAGE_PATH,
        "0",
        SIXEL_BAD_INPUT,
        "load_webp: WebPGetFeatures failed",
        "libwebp fault injection static-get-features");
}

static int
run_fault_static_malloc_case(void)
{
    return run_load_webp_fail_case(
        WEBP_FI_FAIL_STATIC_ALLOC,
        WEBP_IMAGE_PATH,
        "1",
        SIXEL_BAD_ALLOCATION,
        "load_webp: sixel_allocator_malloc() failed.",
        "libwebp fault injection static-malloc");
}

static int
run_fault_lossy_malloc_case(void)
{
    return run_load_webp_fail_case(
        WEBP_FI_FAIL_LOSSY_ALLOC,
        WEBP_IMAGE_PATH,
        "0",
        SIXEL_BAD_ALLOCATION,
        "webp_decode_lossy_to_float32: sixel_allocator_malloc() failed.",
        "libwebp fault injection lossy-malloc");
}

static int
run_fault_animation_canvas_malloc_case(void)
{
    return run_animation_decode_fail_case(
        WEBP_FI_FAIL_ANIMATION_CANVAS_ALLOC,
        SIXEL_BAD_ALLOCATION,
        0,
        "load_with_libwebp: sixel_allocator_malloc() failed.",
        "libwebp fault injection animation-canvas-malloc");
}

static int
run_fault_no_frames_case(void)
{
    return run_animation_decode_fail_case(
        WEBP_FI_FAIL_DECODER_HAS_MORE_FRAMES,
        SIXEL_BAD_INPUT,
        1,
        "load_with_libwebp: no frames in animated WebP stream.",
        "libwebp fault injection decoder-has-more-frames");
}

static int
run_fault_lossy_init_config_case(void)
{
    return run_lossy_fault_case(WEBP_FI_FAIL_LOSSY_INIT_CONFIG);
}

static int
run_fault_lossy_decode_case(void)
{
    return run_lossy_fault_case(WEBP_FI_FAIL_LOSSY_DECODE);
}

static int
run_fault_lossy_yuv_plane_missing_case(void)
{
    return run_lossy_fault_case(WEBP_FI_FAIL_LOSSY_YUV_PLANE_MISSING);
}

static int
run_fault_lossy_yuv_stride_invalid_case(void)
{
    return run_lossy_fault_case(WEBP_FI_FAIL_LOSSY_YUV_STRIDE_INVALID);
}

static int
run_fault_lossy_dimensions_mismatch_case(void)
{
    return run_lossy_fault_case(WEBP_FI_FAIL_LOSSY_DIMENSION_MISMATCH);
}

static void
webpfi_write_u32le(unsigned char *dst, unsigned int value)
{
    dst[0] = (unsigned char)(value & 0xffu);
    dst[1] = (unsigned char)((value >> 8u) & 0xffu);
    dst[2] = (unsigned char)((value >> 16u) & 0xffu);
    dst[3] = (unsigned char)((value >> 24u) & 0xffu);
}

static int
run_extract_subframe_fail_case(unsigned char const *input,
                               size_t input_size,
                               webp_fi_failpoint_t failpoint,
                               SIXELSTATUS expected_status,
                               char const *expected_message,
                               char const *label)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    unsigned char *wrapped_data;
    size_t wrapped_size;
    char const *message;
    int result;

    status = SIXEL_FALSE;
    allocator = NULL;
    wrapped_data = NULL;
    wrapped_size = 0u;
    message = NULL;
    result = 1;

    if (input == NULL || input_size == 0u || expected_message == NULL ||
        label == NULL) {
        return 1;
    }

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: allocator init failed\n", label);
        return 1;
    }

    sixel_helper_set_additional_message(NULL);
    g_webp_fi_failpoint = failpoint;
    status = webp_extract_first_animation_subframe_as_riff(&wrapped_data,
                                                           &wrapped_size,
                                                           input,
                                                           input_size,
                                                           allocator);
    g_webp_fi_failpoint = WEBP_FI_FAIL_NONE;
    if (status != expected_status) {
        fprintf(stderr,
                "%s: expected %d, got %d\n",
                label,
                (int)expected_status,
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
    g_webp_fi_failpoint = WEBP_FI_FAIL_NONE;
    sixel_allocator_free(allocator, wrapped_data);
    sixel_allocator_unref(allocator);
    return result;
}

static int
run_fault_no_anmf_case(void)
{
    unsigned char const input[] = {
        'R', 'I', 'F', 'F',
        0x24, 0x00, 0x00, 0x00,
        'W', 'E', 'B', 'P',
        'V', 'P', '8', 'X',
        0x0a, 0x00, 0x00, 0x00,
        0x02, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00,
        0x00, 0x00, 0x00,
        'A', 'N', 'I', 'M',
        0x06, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00
    };
    return run_extract_subframe_fail_case(
        input,
        sizeof(input),
        WEBP_FI_FAIL_NONE,
        SIXEL_BAD_INPUT,
        "load_with_libwebp: no ANMF chunk found in animated WebP stream.",
        "libwebp fault injection no-anmf");
}

static int
run_fault_anmf_payload_too_small_case(void)
{
    unsigned char const input[] = {
        'R', 'I', 'F', 'F',
        0x20, 0x00, 0x00, 0x00,
        'W', 'E', 'B', 'P',
        'A', 'N', 'M', 'F',
        0x13, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00,
        0x00
    };

    return run_extract_subframe_fail_case(
        input,
        sizeof(input),
        WEBP_FI_FAIL_NONE,
        SIXEL_BAD_INPUT,
        "load_with_libwebp: ANMF payload is too small.",
        "libwebp fault injection anmf-too-small");
}

static int
run_fault_anmf_extract_malloc_case(void)
{
    unsigned char const input[] = {
        'R', 'I', 'F', 'F',
        0x24, 0x00, 0x00, 0x00,
        'W', 'E', 'B', 'P',
        'A', 'N', 'M', 'F',
        0x18, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        'V', 'P', '8', ' ',
        0x00, 0x00, 0x00, 0x00
    };

    return run_extract_subframe_fail_case(
        input,
        sizeof(input),
        WEBP_FI_FAIL_ANMF_EXTRACT_ALLOC,
        SIXEL_BAD_ALLOCATION,
        "load_with_libwebp: sixel_allocator_malloc() failed.",
        "libwebp fault injection anmf-extract-malloc");
}

static int
run_fast_frame_count_limit_case(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    unsigned char *buffer;
    sixel_chunk_t chunk;
    size_t frame_count;
    size_t riff_size;
    size_t total_size;
    size_t offset;
    size_t i;
    char const *message;
    int result;

    status = SIXEL_FALSE;
    allocator = NULL;
    buffer = NULL;
    memset(&chunk, 0, sizeof(chunk));
    frame_count = (size_t)WEBP_FI_MAX_ANIMATION_FRAMES + 1u;
    riff_size = 0u;
    total_size = 0u;
    offset = 0u;
    i = 0u;
    message = NULL;
    result = 1;

    riff_size = 4u;
    riff_size += 8u + 10u;
    riff_size += 8u + 6u;
    if (frame_count > (SIZE_MAX - riff_size) / (8u + 16u)) {
        return 1;
    }
    riff_size += frame_count * (8u + 16u);
    if (riff_size > (size_t)UINT_MAX) {
        return 1;
    }
    total_size = riff_size + 8u;

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "libwebp fault injection frame-limit-fast: allocator init failed\n");
        return 1;
    }

    buffer = (unsigned char *)sixel_allocator_malloc(allocator, total_size);
    if (buffer == NULL) {
        fprintf(stderr,
                "libwebp fault injection frame-limit-fast: allocation failed\n");
        goto cleanup;
    }
    memset(buffer, 0, total_size);

    memcpy(buffer + offset, "RIFF", 4u);
    offset += 4u;
    webpfi_write_u32le(buffer + offset, (unsigned int)riff_size);
    offset += 4u;
    memcpy(buffer + offset, "WEBP", 4u);
    offset += 4u;

    memcpy(buffer + offset, "VP8X", 4u);
    offset += 4u;
    webpfi_write_u32le(buffer + offset, 10u);
    offset += 4u;
    buffer[offset + 0u] = WEBP_VP8X_FLAG_ANIMATION;
    offset += 10u;

    memcpy(buffer + offset, "ANIM", 4u);
    offset += 4u;
    webpfi_write_u32le(buffer + offset, 6u);
    offset += 4u;
    offset += 6u;

    for (i = 0u; i < frame_count; ++i) {
        memcpy(buffer + offset, "ANMF", 4u);
        offset += 4u;
        webpfi_write_u32le(buffer + offset, 16u);
        offset += 4u;
        offset += 16u;
    }

    if (offset != total_size) {
        fprintf(stderr,
                "libwebp fault injection frame-limit-fast: internal size mismatch\n");
        goto cleanup;
    }

    chunk.buffer = buffer;
    chunk.size = total_size;
    chunk.max_size = total_size;
    chunk.allocator = allocator;

    sixel_helper_set_additional_message(NULL);
    status = load_with_libwebp(&chunk,
                               0,
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
    if (status != SIXEL_BAD_INPUT) {
        fprintf(stderr,
                "libwebp fault injection frame-limit-fast: expected SIXEL_BAD_INPUT, got %d\n",
                (int)status);
        goto cleanup;
    }

    message = sixel_helper_get_additional_message();
    if (message == NULL ||
        strstr(message,
               "load_with_libwebp: animation frame count exceeds limit.") ==
            NULL) {
        fprintf(stderr,
                "libwebp fault injection frame-limit-fast: missing diagnostic (actual='%s')\n",
                message != NULL ? message : "(null)");
        goto cleanup;
    }

    result = 0;

cleanup:
    sixel_allocator_free(allocator, buffer);
    sixel_allocator_unref(allocator);
    return result;
}

static int
run_fault_demux_case(void)
{
    return run_demux_fail_case("libwebp fault injection demux");
}
#endif

#if HAVE_WEBP
#define WEBP_FI_TEST_ENTRY(test_name, run_case) \
    int test_name(int argc, char **argv)        \
    {                                            \
        (void)argc;                              \
        (void)argv;                              \
        return run_case();                       \
    }
#else
#define WEBP_FI_TEST_ENTRY(test_name, run_case)          \
    int test_name(int argc, char **argv)                 \
    {                                                     \
        (void)argc;                                       \
        (void)argv;                                       \
        fprintf(stderr, "libwebp loader unavailable\n");  \
        return SIXEL_TEST_SKIP;                           \
    }
#endif

WEBP_FI_TEST_ENTRY(test_loader_0024_loader_libwebp_fault_demux,
                   run_fault_demux_case)
WEBP_FI_TEST_ENTRY(test_loader_0026_loader_libwebp_fault_options_init,
                   run_fault_options_init_case)
WEBP_FI_TEST_ENTRY(test_loader_0027_loader_libwebp_fault_decoder_new,
                   run_fault_decoder_new_case)
WEBP_FI_TEST_ENTRY(test_loader_0028_loader_libwebp_fault_decoder_getinfo,
                   run_fault_decoder_getinfo_case)
WEBP_FI_TEST_ENTRY(test_loader_0029_loader_libwebp_fault_decoder_getnext,
                   run_fault_decoder_getnext_case)
WEBP_FI_TEST_ENTRY(test_loader_0042_loader_libwebp_frame_count_limit_decoder_guard,
                   run_fault_decoder_frame_count_limit_case)
WEBP_FI_TEST_ENTRY(test_loader_0044_loader_libwebp_fault_get_features_animation,
                   run_fault_get_features_animation_case)
WEBP_FI_TEST_ENTRY(test_loader_0032_loader_libwebp_fault_static_rgbinto,
                   run_fault_static_rgbinto_case)
WEBP_FI_TEST_ENTRY(test_loader_0033_loader_libwebp_fault_static_rgbainto,
                   run_fault_static_rgbainto_case)
WEBP_FI_TEST_ENTRY(test_loader_0043_loader_libwebp_fault_get_features_static,
                   run_fault_get_features_static_case)
WEBP_FI_TEST_ENTRY(test_loader_0045_loader_libwebp_fault_static_malloc,
                   run_fault_static_malloc_case)
WEBP_FI_TEST_ENTRY(test_loader_0034_loader_libwebp_fault_lossy_init_config,
                   run_fault_lossy_init_config_case)
WEBP_FI_TEST_ENTRY(test_loader_0035_loader_libwebp_fault_lossy_decode,
                   run_fault_lossy_decode_case)
WEBP_FI_TEST_ENTRY(test_loader_0036_loader_libwebp_fault_lossy_yuv_plane_missing,
                   run_fault_lossy_yuv_plane_missing_case)
WEBP_FI_TEST_ENTRY(test_loader_0037_loader_libwebp_fault_lossy_yuv_stride_invalid,
                   run_fault_lossy_yuv_stride_invalid_case)
WEBP_FI_TEST_ENTRY(test_loader_0038_loader_libwebp_fault_lossy_dimensions_mismatch,
                   run_fault_lossy_dimensions_mismatch_case)
WEBP_FI_TEST_ENTRY(test_loader_0047_loader_libwebp_fault_lossy_malloc,
                   run_fault_lossy_malloc_case)
WEBP_FI_TEST_ENTRY(test_loader_0039_loader_libwebp_fault_no_frames,
                   run_fault_no_frames_case)
WEBP_FI_TEST_ENTRY(test_loader_0046_loader_libwebp_fault_animation_canvas_malloc,
                   run_fault_animation_canvas_malloc_case)
WEBP_FI_TEST_ENTRY(test_loader_0040_loader_libwebp_fault_no_anmf,
                   run_fault_no_anmf_case)
WEBP_FI_TEST_ENTRY(test_loader_0048_loader_libwebp_fault_anmf_payload_too_small,
                   run_fault_anmf_payload_too_small_case)
WEBP_FI_TEST_ENTRY(test_loader_0049_loader_libwebp_fault_anmf_extract_malloc,
                   run_fault_anmf_extract_malloc_case)
WEBP_FI_TEST_ENTRY(test_loader_0041_loader_libwebp_frame_count_limit_fast,
                   run_fast_frame_count_limit_case)

#undef WEBP_FI_TEST_ENTRY

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
