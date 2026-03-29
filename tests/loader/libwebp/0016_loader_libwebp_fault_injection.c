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
    WEBP_FI_FAIL_OPTIONS_INIT,
    WEBP_FI_FAIL_DECODER_NEW,
    WEBP_FI_FAIL_DECODER_GET_INFO,
    WEBP_FI_FAIL_DECODER_GET_NEXT
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

#define WebPAnimDecoderOptionsInit webpfi_WebPAnimDecoderOptionsInit
#define WebPAnimDecoderNew webpfi_WebPAnimDecoderNew
#define WebPAnimDecoderGetInfo webpfi_WebPAnimDecoderGetInfo
#define WebPAnimDecoderGetNext webpfi_WebPAnimDecoderGetNext

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
run_libwebp_fault_injection_test(void)
{
    int status;

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

    return run_decoder_setup_fail_case(
        WEBP_FI_FAIL_DECODER_GET_NEXT,
        "load_with_libwebp: WebPAnimDecoderGetNext failed.",
        "libwebp fault injection decoder-getnext");
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
