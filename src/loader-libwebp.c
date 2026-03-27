/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * libwebp-backed loader helpers extracted from loader.c to keep WebP decoding
 * isolated from unrelated translation units.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if HAVE_WEBP

#include <stdio.h>
#if HAVE_STRING_H
# include <string.h>
#endif
#if HAVE_ERRNO_H
# include <errno.h>
#endif
#if HAVE_LIMITS_H
# include <limits.h>
#endif
#if HAVE_STDLIB_H
# include <stdlib.h>
#endif
#if HAVE_MATH_H
# include <math.h>
#endif
#include <webp/decode.h>
#include <webp/demux.h>

#include <sixel.h>

#include "allocator.h"
#include "cms.h"
#include "chunk.h"
#include "frame.h"
#include "loader-common.h"
#include "loader-libwebp.h"
#include "logger.h"
#include "compat_stub.h"

typedef struct sixel_loader_libwebp_component {
    sixel_loader_component_t base;
    sixel_allocator_t *allocator;
    unsigned int ref;
    int fstatic;
    int fuse_palette;
    int reqcolors;
    int enable_cms;
    int loop_control;
    int has_bgcolor;
    unsigned char bgcolor[3];
    int has_start_frame_no;
    int start_frame_no;
} sixel_loader_libwebp_component_t;

#define WEBP_MAX_DIMENSION        32767
#define WEBP_MAX_IMAGE_PIXELS     ((size_t)268435456u)
#define WEBP_MAX_ANIMATION_FRAMES 65535
#define WEBP_MAX_OUTPUT_FRAMES    ((size_t)262144u)
#define WEBP_MAX_ICC_PROFILE_BYTES ((size_t)1048576u)

static int
webp_convert_embedded_icc_to_srgb(unsigned char *pixels,
                                  int width,
                                  int height,
                                  int pixelformat,
                                  unsigned char const *icc_profile,
                                  size_t icc_profile_length,
                                  sixel_allocator_t *allocator);

static SIXELSTATUS
webp_validate_canvas_limits(int width, int height)
{
    size_t pixel_total;

    pixel_total = 0u;

    if (width <= 0 || height <= 0) {
        sixel_helper_set_additional_message(
            "webp decode: invalid image dimensions.");
        return SIXEL_BAD_INPUT;
    }
    if (width > WEBP_MAX_DIMENSION || height > WEBP_MAX_DIMENSION) {
        sixel_helper_set_additional_message(
            "webp decode: dimensions exceed limit.");
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    if ((size_t)width > SIZE_MAX / (size_t)height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    pixel_total = (size_t)width * (size_t)height;
    if (pixel_total > WEBP_MAX_IMAGE_PIXELS) {
        sixel_helper_set_additional_message(
            "webp decode: image exceeds pixel limit.");
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    return SIXEL_OK;
}

static unsigned int
webp_read_u32le(unsigned char const *p)
{
    if (p == NULL) {
        return 0U;
    }

    return (unsigned int)p[0]
         | ((unsigned int)p[1] << 8)
         | ((unsigned int)p[2] << 16)
         | ((unsigned int)p[3] << 24);
}

static char const *
webp_decode_status_name(VP8StatusCode status)
{
    switch (status) {
    case VP8_STATUS_OK:
        return "VP8_STATUS_OK";
    case VP8_STATUS_OUT_OF_MEMORY:
        return "VP8_STATUS_OUT_OF_MEMORY";
    case VP8_STATUS_INVALID_PARAM:
        return "VP8_STATUS_INVALID_PARAM";
    case VP8_STATUS_BITSTREAM_ERROR:
        return "VP8_STATUS_BITSTREAM_ERROR";
    case VP8_STATUS_UNSUPPORTED_FEATURE:
        return "VP8_STATUS_UNSUPPORTED_FEATURE";
    case VP8_STATUS_SUSPENDED:
        return "VP8_STATUS_SUSPENDED";
    case VP8_STATUS_USER_ABORT:
        return "VP8_STATUS_USER_ABORT";
    case VP8_STATUS_NOT_ENOUGH_DATA:
        return "VP8_STATUS_NOT_ENOUGH_DATA";
    default:
        return "VP8_STATUS_UNKNOWN";
    }
}

static SIXELSTATUS
webp_validate_riff_container(unsigned char const *data, size_t size)
{
    size_t riff_size;
    size_t riff_total_size;
    size_t offset;
    unsigned int chunk_size_u32;
    size_t chunk_size;
    size_t chunk_total_size;
    int saw_chunk;
    unsigned char const *chunk_tag;

    riff_size = 0u;
    riff_total_size = 0u;
    offset = 0u;
    chunk_size_u32 = 0U;
    chunk_size = 0u;
    chunk_total_size = 0u;
    saw_chunk = 0;
    chunk_tag = NULL;

    if (data == NULL || size < 12u) {
        sixel_helper_set_additional_message(
            "webp decode: RIFF header is truncated.");
        return SIXEL_BAD_INPUT;
    }

    if (memcmp(data, "RIFF", 4u) != 0 ||
        memcmp(data + 8u, "WEBP", 4u) != 0) {
        sixel_helper_set_additional_message(
            "webp decode: RIFF/WEBP signature is invalid.");
        return SIXEL_BAD_INPUT;
    }

    riff_size = (size_t)webp_read_u32le(data + 4u);
    if (riff_size < 4u) {
        sixel_helper_set_additional_message(
            "webp decode: RIFF size field is invalid.");
        return SIXEL_BAD_INPUT;
    }
    if (riff_size > SIZE_MAX - 8u) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    riff_total_size = riff_size + 8u;
    if (riff_total_size > size) {
        sixel_helper_set_additional_message(
            "webp decode: RIFF size exceeds input buffer.");
        return SIXEL_BAD_INPUT;
    }

    offset = 12u;
    while (offset < riff_total_size) {
        if (riff_total_size - offset < 8u) {
            sixel_helper_set_additional_message(
                "webp decode: chunk header is truncated.");
            return SIXEL_BAD_INPUT;
        }

        chunk_tag = data + offset;
        if (offset == 12u &&
            memcmp(chunk_tag, "VP8 ", 4u) != 0 &&
            memcmp(chunk_tag, "VP8L", 4u) != 0 &&
            memcmp(chunk_tag, "VP8X", 4u) != 0) {
            sixel_helper_set_additional_message(
                "webp decode: first chunk must be VP8/VP8L/VP8X.");
            return SIXEL_BAD_INPUT;
        }

        chunk_size_u32 = webp_read_u32le(data + offset + 4u);
        chunk_size = (size_t)chunk_size_u32;
        if (memcmp(chunk_tag, "VP8X", 4u) == 0 && chunk_size != 10u) {
            sixel_helper_set_additional_message(
                "webp decode: VP8X chunk size is invalid.");
            return SIXEL_BAD_INPUT;
        }
        if (memcmp(chunk_tag, "ANIM", 4u) == 0 && chunk_size != 6u) {
            sixel_helper_set_additional_message(
                "webp decode: ANIM chunk size is invalid.");
            return SIXEL_BAD_INPUT;
        }
        if (memcmp(chunk_tag, "ANMF", 4u) == 0 && chunk_size < 16u) {
            sixel_helper_set_additional_message(
                "webp decode: ANMF chunk size is too small.");
            return SIXEL_BAD_INPUT;
        }
        if (chunk_size > SIZE_MAX - 8u - offset) {
            return SIXEL_BAD_INTEGER_OVERFLOW;
        }

        chunk_total_size = 8u + chunk_size;
        if ((chunk_size_u32 & 1u) != 0u) {
            if (chunk_total_size == SIZE_MAX) {
                return SIXEL_BAD_INTEGER_OVERFLOW;
            }
            chunk_total_size += 1u;
        }

        if (chunk_total_size > riff_total_size - offset) {
            if ((chunk_size_u32 & 1u) != 0u &&
                chunk_total_size - 1u == riff_total_size - offset) {
                sixel_helper_set_additional_message(
                    "webp decode: odd-sized chunk is missing padding byte.");
                return SIXEL_BAD_INPUT;
            }
            sixel_helper_set_additional_message(
                "webp decode: chunk payload exceeds RIFF size.");
            return SIXEL_BAD_INPUT;
        }
        if ((chunk_size_u32 & 1u) != 0u &&
            data[offset + 8u + chunk_size] != 0u) {
            sixel_helper_set_additional_message(
                "webp decode: odd-sized chunk has non-zero padding byte.");
            return SIXEL_BAD_INPUT;
        }

        saw_chunk = 1;
        offset += chunk_total_size;
    }

    if (!saw_chunk) {
        sixel_helper_set_additional_message(
            "webp decode: no RIFF chunks found.");
        return SIXEL_BAD_INPUT;
    }

    return SIXEL_OK;
}

static float
webp_clamp_unit_float(float value)
{
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

static float const g_webp_srgb_decode_lut[256] = {
    0.000000000f,
    0.000303527f,
    0.000607054f,
    0.000910581f,
    0.001214108f,
    0.001517635f,
    0.001821162f,
    0.002124689f,
    0.002428216f,
    0.002731743f,
    0.003035270f,
    0.003346536f,
    0.003676507f,
    0.004024717f,
    0.004391442f,
    0.004776953f,
    0.005181517f,
    0.005605392f,
    0.006048833f,
    0.006512091f,
    0.006995410f,
    0.007499032f,
    0.008023193f,
    0.008568126f,
    0.009134059f,
    0.009721217f,
    0.010329823f,
    0.010960094f,
    0.011612245f,
    0.012286488f,
    0.012983032f,
    0.013702083f,
    0.014443844f,
    0.015208514f,
    0.015996293f,
    0.016807376f,
    0.017641954f,
    0.018500220f,
    0.019382361f,
    0.020288563f,
    0.021219010f,
    0.022173885f,
    0.023153366f,
    0.024157632f,
    0.025186860f,
    0.026241222f,
    0.027320892f,
    0.028426040f,
    0.029556834f,
    0.030713444f,
    0.031896033f,
    0.033104767f,
    0.034339807f,
    0.035601315f,
    0.036889450f,
    0.038204372f,
    0.039546235f,
    0.040915197f,
    0.042311411f,
    0.043735029f,
    0.045186204f,
    0.046665086f,
    0.048171824f,
    0.049706566f,
    0.051269458f,
    0.052860647f,
    0.054480276f,
    0.056128490f,
    0.057805430f,
    0.059511238f,
    0.061246054f,
    0.063010018f,
    0.064803267f,
    0.066625939f,
    0.068478170f,
    0.070360096f,
    0.072271851f,
    0.074213568f,
    0.076185381f,
    0.078187422f,
    0.080219820f,
    0.082282707f,
    0.084376212f,
    0.086500462f,
    0.088655586f,
    0.090841711f,
    0.093058963f,
    0.095307467f,
    0.097587347f,
    0.099898728f,
    0.102241733f,
    0.104616484f,
    0.107023103f,
    0.109461711f,
    0.111932428f,
    0.114435374f,
    0.116970668f,
    0.119538428f,
    0.122138772f,
    0.124771818f,
    0.127437680f,
    0.130136477f,
    0.132868322f,
    0.135633330f,
    0.138431615f,
    0.141263291f,
    0.144128471f,
    0.147027266f,
    0.149959790f,
    0.152926152f,
    0.155926464f,
    0.158960835f,
    0.162029376f,
    0.165132195f,
    0.168269400f,
    0.171441101f,
    0.174647404f,
    0.177888416f,
    0.181164244f,
    0.184474995f,
    0.187820772f,
    0.191201683f,
    0.194617830f,
    0.198069320f,
    0.201556254f,
    0.205078736f,
    0.208636870f,
    0.212230757f,
    0.215860500f,
    0.219526200f,
    0.223227957f,
    0.226965874f,
    0.230740049f,
    0.234550582f,
    0.238397574f,
    0.242281122f,
    0.246201327f,
    0.250158285f,
    0.254152094f,
    0.258182853f,
    0.262250658f,
    0.266355605f,
    0.270497791f,
    0.274677312f,
    0.278894263f,
    0.283148740f,
    0.287440838f,
    0.291770650f,
    0.296138271f,
    0.300543794f,
    0.304987314f,
    0.309468923f,
    0.313988713f,
    0.318546778f,
    0.323143209f,
    0.327778098f,
    0.332451536f,
    0.337163615f,
    0.341914425f,
    0.346704056f,
    0.351532600f,
    0.356400144f,
    0.361306780f,
    0.366252596f,
    0.371237680f,
    0.376262123f,
    0.381326011f,
    0.386429434f,
    0.391572478f,
    0.396755231f,
    0.401977780f,
    0.407240212f,
    0.412542613f,
    0.417885071f,
    0.423267670f,
    0.428690497f,
    0.434153636f,
    0.439657174f,
    0.445201195f,
    0.450785783f,
    0.456411023f,
    0.462077000f,
    0.467783796f,
    0.473531496f,
    0.479320183f,
    0.485149940f,
    0.491020850f,
    0.496932995f,
    0.502886458f,
    0.508881321f,
    0.514917665f,
    0.520995573f,
    0.527115126f,
    0.533276404f,
    0.539479489f,
    0.545724461f,
    0.552011402f,
    0.558340390f,
    0.564711506f,
    0.571124829f,
    0.577580440f,
    0.584078418f,
    0.590618841f,
    0.597201788f,
    0.603827339f,
    0.610495571f,
    0.617206562f,
    0.623960392f,
    0.630757136f,
    0.637596874f,
    0.644479682f,
    0.651405637f,
    0.658374817f,
    0.665387298f,
    0.672443157f,
    0.679542470f,
    0.686685312f,
    0.693871761f,
    0.701101892f,
    0.708375780f,
    0.715693501f,
    0.723055129f,
    0.730460740f,
    0.737910409f,
    0.745404210f,
    0.752942217f,
    0.760524505f,
    0.768151147f,
    0.775822218f,
    0.783537792f,
    0.791297940f,
    0.799102738f,
    0.806952258f,
    0.814846572f,
    0.822785754f,
    0.830769877f,
    0.838799012f,
    0.846873232f,
    0.854992608f,
    0.863157213f,
    0.871367119f,
    0.879622397f,
    0.887923118f,
    0.896269353f,
    0.904661174f,
    0.913098652f,
    0.921581856f,
    0.930110858f,
    0.938685728f,
    0.947306537f,
    0.955973353f,
    0.964686248f,
    0.973445290f,
    0.982250550f,
    0.991102097f,
    1.000000000f
};

static float
webp_decode_srgb_byte(unsigned char value)
{
    return g_webp_srgb_decode_lut[(int)value];
}

static float
webp_decode_srgb_unit(float value)
{
    float scaled;
    int index;
    float frac;

    value = webp_clamp_unit_float(value);
    scaled = value * 255.0f;
    index = (int)scaled;
    if (index <= 0) {
        return g_webp_srgb_decode_lut[0];
    }
    if (index >= 255) {
        return g_webp_srgb_decode_lut[255];
    }

    frac = scaled - (float)index;
    return g_webp_srgb_decode_lut[index] +
           (g_webp_srgb_decode_lut[index + 1] -
            g_webp_srgb_decode_lut[index]) * frac;
}

static void
webp_resolve_background_linear(float bg_linear[3], unsigned char *bgcolor)
{
    int background_colorspace;

    background_colorspace = loader_background_colorspace();
    bg_linear[0] = 0.0f;
    bg_linear[1] = 0.0f;
    bg_linear[2] = 0.0f;
    if (bgcolor == NULL) {
        return;
    }

    if (background_colorspace == SIXEL_COLORSPACE_LINEAR) {
        bg_linear[0] = (float)bgcolor[0] / 255.0f;
        bg_linear[1] = (float)bgcolor[1] / 255.0f;
        bg_linear[2] = (float)bgcolor[2] / 255.0f;
        return;
    }

    bg_linear[0] = webp_decode_srgb_byte(bgcolor[0]);
    bg_linear[1] = webp_decode_srgb_byte(bgcolor[1]);
    bg_linear[2] = webp_decode_srgb_byte(bgcolor[2]);
}

static SIXELSTATUS
webp_blend_rgba_background_linear(sixel_frame_t *frame,
                                  unsigned char *bgcolor,
                                  sixel_allocator_t *allocator)
{
    unsigned char *src;
    float *dst;
    size_t pixel_count;
    size_t bytes;
    size_t i;
    size_t src_offset;
    size_t dst_offset;
    float alpha;
    float bg_linear[3];
    int has_transparency;

    src = NULL;
    dst = NULL;
    pixel_count = 0u;
    bytes = 0u;
    i = 0u;
    src_offset = 0u;
    dst_offset = 0u;
    alpha = 0.0f;
    bg_linear[0] = 0.0f;
    bg_linear[1] = 0.0f;
    bg_linear[2] = 0.0f;
    has_transparency = 0;

    if (frame == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (bgcolor == NULL) {
        return SIXEL_OK;
    }
    if (frame->pixelformat != SIXEL_PIXELFORMAT_RGBA8888) {
        return SIXEL_OK;
    }
    if (frame->width <= 0 || frame->height <= 0) {
        return SIXEL_BAD_INPUT;
    }
    if ((size_t)frame->width > SIZE_MAX / (size_t)frame->height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    src = frame->pixels.u8ptr;
    if (src == NULL) {
        return SIXEL_BAD_INPUT;
    }

    pixel_count = (size_t)frame->width * (size_t)frame->height;
    for (i = 0u; i < pixel_count; ++i) {
        if (src[i * 4u + 3u] != 255u) {
            has_transparency = 1;
            break;
        }
    }
    if (!has_transparency) {
        return SIXEL_OK;
    }

    if (pixel_count > SIZE_MAX / (3u * sizeof(float))) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    bytes = pixel_count * 3u * sizeof(float);
    dst = (float *)sixel_allocator_malloc(allocator, bytes);
    if (dst == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    webp_resolve_background_linear(bg_linear, bgcolor);
    for (i = 0u; i < pixel_count; ++i) {
        src_offset = i * 4u;
        dst_offset = i * 3u;
        alpha = (float)src[src_offset + 3u] / 255.0f;

        dst[dst_offset + 0u] =
            webp_decode_srgb_byte(src[src_offset + 0u]) * alpha
            + bg_linear[0] * (1.0f - alpha);
        dst[dst_offset + 1u] =
            webp_decode_srgb_byte(src[src_offset + 1u]) * alpha
            + bg_linear[1] * (1.0f - alpha);
        dst[dst_offset + 2u] =
            webp_decode_srgb_byte(src[src_offset + 2u]) * alpha
            + bg_linear[2] * (1.0f - alpha);
    }

    sixel_allocator_free(allocator, frame->pixels.u8ptr);
    frame->pixels.u8ptr = NULL;
    sixel_frame_set_pixels_float32(frame, dst);
    frame->pixelformat = SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
    frame->colorspace = SIXEL_COLORSPACE_LINEAR;
    return SIXEL_OK;
}

static SIXELSTATUS
webp_decode_lossy_to_float32(unsigned char **result,
                             unsigned char *data,
                             size_t datasize,
                             int width,
                             int height,
                             int has_alpha,
                             int *ppixelformat,
                             int enable_cms,
                             unsigned char const *icc_profile,
                             size_t icc_profile_length,
                             int *pcms_converted,
                             unsigned char *bgcolor,
                             sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    WebPDecoderConfig config;
    VP8StatusCode decode_status;
    float *float_pixels;
    size_t pixel_count;
    size_t float_bytes;
    uint8_t *y_plane;
    uint8_t *u_plane;
    uint8_t *v_plane;
    uint8_t *a_plane;
    int y_stride;
    int u_stride;
    int v_stride;
    int a_stride;
    int x;
    int y;
    size_t offset;
    float y_sample;
    float u_sample;
    float v_sample;
    float r;
    float g;
    float b;
    float alpha;
    float bg_linear[3];
    int has_bgcolor;
    int blended_in_linear;
    int has_transparency;
    int config_initialized;
    int cms_converted;
    char error_message[128];

    status = SIXEL_BAD_INPUT;
    memset(&config, 0, sizeof(config));
    decode_status = VP8_STATUS_OK;
    float_pixels = NULL;
    pixel_count = 0u;
    float_bytes = 0u;
    y_plane = NULL;
    u_plane = NULL;
    v_plane = NULL;
    a_plane = NULL;
    y_stride = 0;
    u_stride = 0;
    v_stride = 0;
    a_stride = 0;
    x = 0;
    y = 0;
    offset = 0u;
    y_sample = 0.0f;
    u_sample = 0.0f;
    v_sample = 0.0f;
    r = 0.0f;
    g = 0.0f;
    b = 0.0f;
    alpha = 0.0f;
    bg_linear[0] = 0.0f;
    bg_linear[1] = 0.0f;
    bg_linear[2] = 0.0f;
    has_bgcolor = 0;
    blended_in_linear = 0;
    has_transparency = 0;
    config_initialized = 0;
    cms_converted = 0;
    memset(error_message, 0, sizeof(error_message));

    if (result == NULL || ppixelformat == NULL || allocator == NULL ||
        data == NULL || datasize == 0u || width <= 0 || height <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (!WebPInitDecoderConfig(&config)) {
        sixel_helper_set_additional_message(
            "webp_decode_lossy_to_float32: WebPInitDecoderConfig failed.");
        return SIXEL_WEBP_ERROR;
    }
    config_initialized = 1;
    config.options.use_threads = 1;
    config.output.colorspace = has_alpha ? MODE_YUVA : MODE_YUV;

    decode_status = WebPDecode(data, datasize, &config);
    if (decode_status != VP8_STATUS_OK) {
        (void)snprintf(error_message,
                       sizeof(error_message),
                       "webp_decode_lossy_to_float32: WebPDecode failed (%s:%d).",
                       webp_decode_status_name(decode_status),
                       (int)decode_status);
        sixel_helper_set_additional_message(error_message);
        status = SIXEL_WEBP_ERROR;
        goto end;
    }

    y_plane = config.output.u.YUVA.y;
    u_plane = config.output.u.YUVA.u;
    v_plane = config.output.u.YUVA.v;
    a_plane = config.output.u.YUVA.a;
    y_stride = config.output.u.YUVA.y_stride;
    u_stride = config.output.u.YUVA.u_stride;
    v_stride = config.output.u.YUVA.v_stride;
    a_stride = config.output.u.YUVA.a_stride;
    if (y_plane == NULL || u_plane == NULL || v_plane == NULL) {
        sixel_helper_set_additional_message(
            "webp_decode_lossy_to_float32: YUV plane is missing.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (config.output.width != width || config.output.height != height) {
        sixel_helper_set_additional_message(
            "webp_decode_lossy_to_float32: decoded dimensions mismatch.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (y_stride <= 0 || u_stride <= 0 || v_stride <= 0 ||
        (has_alpha && a_plane != NULL && a_stride <= 0)) {
        sixel_helper_set_additional_message(
            "webp_decode_lossy_to_float32: YUV plane stride is invalid.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if ((size_t)width > SIZE_MAX / (size_t)height) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto end;
    }

    pixel_count = (size_t)width * (size_t)height;
    if (pixel_count > SIZE_MAX / (3u * sizeof(float))) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto end;
    }
    float_bytes = pixel_count * 3u * sizeof(float);
    float_pixels = (float *)sixel_allocator_malloc(allocator, float_bytes);
    if (float_pixels == NULL) {
        sixel_helper_set_additional_message(
            "webp_decode_lossy_to_float32: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    has_bgcolor = (bgcolor != NULL);

    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            y_sample = (float)y_plane[(size_t)y * (size_t)y_stride + (size_t)x]
                     - 16.0f;
            if (y_sample < 0.0f) {
                y_sample = 0.0f;
            }
            u_sample = (float)u_plane[(size_t)(y >> 1) * (size_t)u_stride
                                      + (size_t)(x >> 1)] - 128.0f;
            v_sample = (float)v_plane[(size_t)(y >> 1) * (size_t)v_stride
                                      + (size_t)(x >> 1)] - 128.0f;

            r = (1.16438356f * y_sample + 1.59602678f * v_sample) / 255.0f;
            g = (1.16438356f * y_sample - 0.39176229f * u_sample
                 - 0.81296765f * v_sample) / 255.0f;
            b = (1.16438356f * y_sample + 2.01723214f * u_sample) / 255.0f;

            r = webp_clamp_unit_float(r);
            g = webp_clamp_unit_float(g);
            b = webp_clamp_unit_float(b);

            offset = ((size_t)y * (size_t)width + (size_t)x) * 3u;
            float_pixels[offset + 0u] = r;
            float_pixels[offset + 1u] = g;
            float_pixels[offset + 2u] = b;
        }
    }

    cms_converted = 0;
    if (enable_cms) {
        cms_converted = webp_convert_embedded_icc_to_srgb(
            (unsigned char *)float_pixels,
            width,
            height,
            SIXEL_PIXELFORMAT_RGBFLOAT32,
            icc_profile,
            icc_profile_length,
            allocator);
    }

    if (has_alpha && a_plane != NULL && has_bgcolor) {
        for (y = 0; y < height; ++y) {
            for (x = 0; x < width; ++x) {
                if (a_plane[(size_t)y * (size_t)a_stride + (size_t)x] != 255u) {
                    has_transparency = 1;
                    break;
                }
            }
            if (has_transparency) {
                break;
            }
        }
    }

    if (has_transparency) {
        webp_resolve_background_linear(bg_linear, bgcolor);
        for (y = 0; y < height; ++y) {
            for (x = 0; x < width; ++x) {
                offset = ((size_t)y * (size_t)width + (size_t)x) * 3u;
                alpha = (float)a_plane[(size_t)y * (size_t)a_stride + (size_t)x]
                      / 255.0f;
                float_pixels[offset + 0u] =
                    webp_decode_srgb_unit(float_pixels[offset + 0u]) * alpha
                    + bg_linear[0] * (1.0f - alpha);
                float_pixels[offset + 1u] =
                    webp_decode_srgb_unit(float_pixels[offset + 1u]) * alpha
                    + bg_linear[1] * (1.0f - alpha);
                float_pixels[offset + 2u] =
                    webp_decode_srgb_unit(float_pixels[offset + 2u]) * alpha
                    + bg_linear[2] * (1.0f - alpha);
            }
        }
        blended_in_linear = 1;
    }
    if (pcms_converted != NULL) {
        *pcms_converted = cms_converted;
    }

    *result = (unsigned char *)float_pixels;
    if (blended_in_linear) {
        *ppixelformat = SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
    } else {
        *ppixelformat = SIXEL_PIXELFORMAT_RGBFLOAT32;
    }
    float_pixels = NULL;
    status = SIXEL_OK;

end:
    if (float_pixels != NULL) {
        sixel_allocator_free(allocator, float_pixels);
    }
    if (config_initialized) {
        WebPFreeDecBuffer(&config.output);
    }
    return status;
}

/*
 * Decode a WebP buffer into an RGB(A) pixel buffer managed by libsixel.
 *
 * The steps are:
 *   1) Probe the WebP bitstream for dimensions and alpha flags.
 *   2) Allocate the output buffer from the sixel allocator.
 *   3) Decode lossy streams via YUV(A)-plane decode when background-aware
 *      composition is required.
 *   4) Decode remaining streams (including lossy+alpha without background)
 *      into RGB/RGBA bytes.
 */
static SIXELSTATUS
load_webp(unsigned char **result,
          unsigned char *data,
          size_t datasize,
          int *pwidth,
          int *pheight,
          int *ppixelformat,
          int enable_cms,
          unsigned char const *icc_profile,
          size_t icc_profile_length,
          int *pcms_converted,
          unsigned char *bgcolor,
          sixel_allocator_t *allocator,
          WebPBitstreamFeatures const *known_features)
{
    SIXELSTATUS status;
    WebPBitstreamFeatures features;
    int bytes_per_pixel;
    int cms_converted;
    char const *force_rgb_env;
    int force_rgb_decode;
    size_t stride;
    size_t size;
    VP8StatusCode feature_status;
    char error_message[128];
    int use_known_features;

    status = SIXEL_BAD_INPUT;
    cms_converted = 0;
    force_rgb_env = NULL;
    force_rgb_decode = 0;
    feature_status = VP8_STATUS_OK;
    memset(error_message, 0, sizeof(error_message));
    use_known_features = 0;
    if (result == NULL || data == NULL || datasize == 0u ||
        pwidth == NULL || pheight == NULL || ppixelformat == NULL ||
        allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *result = NULL;

    if (known_features != NULL) {
        features = *known_features;
        use_known_features = 1;
    }
    if (!use_known_features) {
        status = webp_validate_riff_container(data, datasize);
        if (SIXEL_FAILED(status)) {
            return status;
        }

        feature_status = WebPGetFeatures(data, datasize, &features);
        if (feature_status != VP8_STATUS_OK) {
            (void)snprintf(error_message,
                           sizeof(error_message),
                           "load_webp: WebPGetFeatures failed (%s:%d).",
                           webp_decode_status_name(feature_status),
                           (int)feature_status);
            sixel_helper_set_additional_message(error_message);
            return status;
        }
    }

    if (features.width > INT_MAX || features.height > INT_MAX) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    status = webp_validate_canvas_limits(features.width, features.height);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    *pwidth = features.width;
    *pheight = features.height;

    /*
     * Keep a test/debug escape hatch so regression tests can compare the
     * lossy YUV path against the legacy RGB decode path.
     */
    force_rgb_env = sixel_compat_getenv(
        "SIXEL_LOADER_LIBWEBP_LOSSY_USE_RGB_DECODE");
    if (force_rgb_env != NULL &&
        (force_rgb_env[0] == '1' || force_rgb_env[0] == 'y' ||
         force_rgb_env[0] == 'Y' || force_rgb_env[0] == 't' ||
         force_rgb_env[0] == 'T')) {
        force_rgb_decode = 1;
    }

    if (features.format == 1 && !force_rgb_decode &&
        !(features.has_alpha && bgcolor == NULL)) {
        sixel_trace_topic_message(
            "webp_decode",
            "static decode path=lossy_yuva width=%d height=%d has_alpha=%d "
            "has_bgcolor=%d force_rgb=%d",
            *pwidth,
            *pheight,
            features.has_alpha,
            bgcolor != NULL ? 1 : 0,
            force_rgb_decode);
        return webp_decode_lossy_to_float32(result,
                                            data,
                                            datasize,
                                            *pwidth,
                                            *pheight,
                                            features.has_alpha,
                                            ppixelformat,
                                            enable_cms,
                                            icc_profile,
                                            icc_profile_length,
                                            pcms_converted,
                                            bgcolor,
                                            allocator);
    }

    sixel_trace_topic_message(
        "webp_decode",
        "static decode path=%s width=%d height=%d has_alpha=%d has_bgcolor=%d "
        "force_rgb=%d",
        features.has_alpha ? "rgba_u8" : "rgb_u8",
        *pwidth,
        *pheight,
        features.has_alpha,
        bgcolor != NULL ? 1 : 0,
        force_rgb_decode);

    bytes_per_pixel = features.has_alpha ? 4 : 3;
    *ppixelformat = features.has_alpha ?
        SIXEL_PIXELFORMAT_RGBA8888 : SIXEL_PIXELFORMAT_RGB888;

    if ((size_t)*pwidth > SIZE_MAX / (size_t)bytes_per_pixel) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    stride = (size_t)*pwidth * (size_t)bytes_per_pixel;
    if ((size_t)*pheight > 0 && stride > SIZE_MAX / (size_t)*pheight) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    size = stride * (size_t)*pheight;
    *result = (unsigned char *)sixel_allocator_malloc(allocator, size);
    if (*result == NULL) {
        sixel_helper_set_additional_message(
            "load_webp: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    if (features.has_alpha) {
        if (WebPDecodeRGBAInto(data, datasize, *result, size,
                               (int)stride) == NULL) {
            sixel_helper_set_additional_message(
                "load_webp: WebPDecodeRGBAInto failed.");
            status = SIXEL_BAD_INPUT;
            goto end;
        }
    } else {
        if (WebPDecodeRGBInto(data, datasize, *result, size,
                              (int)stride) == NULL) {
            sixel_helper_set_additional_message(
                "load_webp: WebPDecodeRGBInto failed.");
            status = SIXEL_BAD_INPUT;
            goto end;
        }
    }

    if (enable_cms) {
        cms_converted = webp_convert_embedded_icc_to_srgb(*result,
                                                          *pwidth,
                                                          *pheight,
                                                          *ppixelformat,
                                                          icc_profile,
                                                          icc_profile_length,
                                                          allocator);
    }
    if (pcms_converted != NULL) {
        *pcms_converted = cms_converted;
    }

    status = SIXEL_OK;

end:
    if (SIXEL_FAILED(status) && *result != NULL) {
        sixel_allocator_free(allocator, *result);
        *result = NULL;
    }
    return status;
}

/*
 * Extract embedded ICC profile bytes from a WebP container if present.
 *
 * The ICC payload is stored in the ICCP chunk. This helper copies the
 * payload into allocator-managed memory so callers can safely use it after
 * the demuxer has been deleted. Oversized profiles are ignored to avoid
 * unbounded allocations in CMS-enabled decode paths.
 */
static void
webp_extract_icc_profile(WebPDemuxer const *demux,
                         unsigned char **icc_profile,
                         size_t *icc_profile_length,
                         sixel_allocator_t *allocator)
{
    WebPChunkIterator chunk_iter;
    unsigned int format_flags;

    chunk_iter = (WebPChunkIterator){ 0 };
    format_flags = 0U;

    *icc_profile = NULL;
    *icc_profile_length = 0U;

    if (demux == NULL || allocator == NULL) {
        return;
    }

    format_flags = WebPDemuxGetI(demux, WEBP_FF_FORMAT_FLAGS);
    if ((format_flags & ICCP_FLAG) == 0U) {
        goto cleanup;
    }

    if (!WebPDemuxGetChunk(demux, "ICCP", 1, &chunk_iter)) {
        goto cleanup;
    }
    if (chunk_iter.chunk.bytes == NULL || chunk_iter.chunk.size == 0U) {
        goto cleanup;
    }
    if (chunk_iter.chunk.size > WEBP_MAX_ICC_PROFILE_BYTES) {
        sixel_trace_topic_message(
            "webp_decode",
            "embedded ICC profile too large (%zu bytes); skip CMS conversion",
            chunk_iter.chunk.size);
        goto cleanup;
    }

    *icc_profile = (unsigned char *)sixel_allocator_malloc(
        allocator,
        chunk_iter.chunk.size);
    if (*icc_profile == NULL) {
        goto cleanup;
    }

    memcpy(*icc_profile, chunk_iter.chunk.bytes, chunk_iter.chunk.size);
    *icc_profile_length = chunk_iter.chunk.size;

cleanup:
    if (chunk_iter.chunk.bytes != NULL) {
        WebPDemuxReleaseChunkIterator(&chunk_iter);
    }
}

/*
 * Convert decoded WebP pixels from embedded ICC profile space to sRGB.
 *
 * The alpha channel is preserved when RGBA pixels are provided. If ICC data
 * is missing or invalid, the decoded pixels remain unchanged.
 */
static int
webp_convert_embedded_icc_to_srgb(unsigned char *pixels,
                                  int width,
                                  int height,
                                  int pixelformat,
                                  unsigned char const *icc_profile,
                                  size_t icc_profile_length,
                                  sixel_allocator_t *allocator)
{
#if HAVE_LCMS2
    sixel_cms_profile_t * src_profile;
    sixel_cms_profile_t * dst_profile;
    sixel_cms_transform_t * transform;
    sixel_cms_pixel_format_t src_type;
    sixel_cms_pixel_format_t dst_type;
    int transform_flags;
    int converted;
    size_t pixel_count;

    (void)allocator;

    src_profile = NULL;
    dst_profile = NULL;
    transform = NULL;
    src_type = SIXEL_CMS_PIXELFORMAT_RGB_8;
    dst_type = SIXEL_CMS_PIXELFORMAT_RGB_8;
    transform_flags = 0U;
    converted = 0;
    pixel_count = 0U;

    if (pixels == NULL || width <= 0 || height <= 0 ||
        icc_profile == NULL || icc_profile_length == 0U) {
        return 0;
    }

    if (pixelformat == SIXEL_PIXELFORMAT_RGBA8888) {
        src_type = SIXEL_CMS_PIXELFORMAT_RGBA_8;
        dst_type = SIXEL_CMS_PIXELFORMAT_RGBA_8;
        transform_flags = SIXEL_CMS_TRANSFORM_COPY_ALPHA;
    } else if (pixelformat == SIXEL_PIXELFORMAT_RGBFLOAT32
               || pixelformat == SIXEL_PIXELFORMAT_LINEARRGBFLOAT32) {
        src_type = SIXEL_CMS_PIXELFORMAT_RGB_F32;
        dst_type = SIXEL_CMS_PIXELFORMAT_RGB_F32;
    } else if (pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        return 0;
    }

    src_profile = sixel_cms_open_profile_from_mem(icc_profile, icc_profile_length);
    if (src_profile == NULL) {
        return 0;
    }
    dst_profile = sixel_cms_create_srgb_profile();
    if (dst_profile == NULL) {
        goto cleanup;
    }

    transform = sixel_cms_create_transform(src_profile,
                                   src_type,
                                   dst_profile,
                                   dst_type,
                                   transform_flags);
    if (transform == NULL) {
        goto cleanup;
    }

    pixel_count = (size_t)width * (size_t)height;
    converted = sixel_cms_do_transform(transform, pixels, pixels, pixel_count);

cleanup:
    if (transform != NULL) {
        sixel_cms_delete_transform(transform);
    }
    if (dst_profile != NULL) {
        sixel_cms_close_profile(dst_profile);
    }
    if (src_profile != NULL) {
        sixel_cms_close_profile(src_profile);
    }
    return converted;
#else
    unsigned char *rgb_pixels;
    size_t pixel_count;
    size_t rgb_bytes;
    size_t i;
    size_t src_offset;
    size_t dst_offset;
    int converted;

    rgb_pixels = NULL;
    pixel_count = 0u;
    rgb_bytes = 0u;
    i = 0u;
    src_offset = 0u;
    dst_offset = 0u;
    converted = 0;

    if (pixels == NULL || width <= 0 || height <= 0 ||
        allocator == NULL ||
        icc_profile == NULL || icc_profile_length == 0u) {
        return 0;
    }

    if (pixelformat == SIXEL_PIXELFORMAT_RGB888
            || pixelformat == SIXEL_PIXELFORMAT_RGBFLOAT32
            || pixelformat == SIXEL_PIXELFORMAT_LINEARRGBFLOAT32) {
        return sixel_cms_convert_to_srgb_with_profile_bytes(
            pixels,
            width,
            height,
            pixelformat,
            icc_profile,
            icc_profile_length);
    }
    if (pixelformat != SIXEL_PIXELFORMAT_RGBA8888) {
        return 0;
    }
    if ((size_t)width > SIZE_MAX / (size_t)height) {
        return 0;
    }
    pixel_count = (size_t)width * (size_t)height;
    if (pixel_count > SIZE_MAX / 3u) {
        return 0;
    }
    rgb_bytes = pixel_count * 3u;
    rgb_pixels = (unsigned char *)sixel_allocator_malloc(allocator, rgb_bytes);
    if (rgb_pixels == NULL) {
        return 0;
    }

    for (i = 0u; i < pixel_count; ++i) {
        src_offset = i * 4u;
        dst_offset = i * 3u;
        rgb_pixels[dst_offset + 0u] = pixels[src_offset + 0u];
        rgb_pixels[dst_offset + 1u] = pixels[src_offset + 1u];
        rgb_pixels[dst_offset + 2u] = pixels[src_offset + 2u];
    }

    converted = sixel_cms_convert_to_srgb_with_profile_bytes(
        rgb_pixels,
        width,
        height,
        SIXEL_PIXELFORMAT_RGB888,
        icc_profile,
        icc_profile_length);
    if (converted) {
        for (i = 0u; i < pixel_count; ++i) {
            src_offset = i * 3u;
            dst_offset = i * 4u;
            pixels[dst_offset + 0u] = rgb_pixels[src_offset + 0u];
            pixels[dst_offset + 1u] = rgb_pixels[src_offset + 1u];
            pixels[dst_offset + 2u] = rgb_pixels[src_offset + 2u];
        }
    }

    sixel_allocator_free(allocator, rgb_pixels);
    return converted;
#endif
}


/*
 * Parse a VP8L payload header and detect whether the color indexing
 * transform is present.
 */
static int
vp8l_payload_uses_color_indexing(unsigned char const *data, size_t size)
{
    unsigned int bitbuf;
    int bits;
    size_t pos;
    unsigned int value;
    int transform_type;

    bitbuf = 0U;
    bits = 0;
    pos = 0U;
    value = 0U;
    transform_type = 0;

    if (data == NULL || size < 5U) {
        return 0;
    }

    if (size >= 13U &&
        data[0] == 'V' &&
        data[1] == 'P' &&
        data[2] == '8' &&
        data[3] == 'L') {
        data += 8;
        size -= 8U;
    }

    if (data[0] != 0x2fU) {
        return 0;
    }

    pos = 5U;

    for (;;) {
        while (bits < 1) {
            if (pos >= size) {
                return 0;
            }
            bitbuf |= (unsigned int)data[pos] << bits;
            bits += 8;
            pos++;
        }
        value = bitbuf & 0x1U;
        bitbuf >>= 1;
        bits -= 1;
        if (value == 0U) {
            break;
        }

        while (bits < 2) {
            if (pos >= size) {
                return 0;
            }
            bitbuf |= (unsigned int)data[pos] << bits;
            bits += 8;
            pos++;
        }
        transform_type = (int)(bitbuf & 0x3U);
        bitbuf >>= 2;
        bits -= 2;

        if (transform_type == 3) {
            return 1;
        }

        if (transform_type == 0 || transform_type == 1) {
            while (bits < 3) {
                if (pos >= size) {
                    return 0;
                }
                bitbuf |= (unsigned int)data[pos] << bits;
                bits += 8;
                pos++;
            }
            bitbuf >>= 3;
            bits -= 3;
        } else if (transform_type == 2) {
            /* subtract-green transform carries no additional payload bits */
        }
    }

    return 0;
}


static int
webp_input_is_indexed(WebPDemuxer const *demux)
{
    WebPIterator iter;
    int frame_count;
    int frame_index;
    int indexed;

    iter = (WebPIterator){ 0 };
    frame_count = 0;
    frame_index = 0;
    indexed = 0;

    if (demux == NULL) {
        return 0;
    }

    frame_count = (int)WebPDemuxGetI(demux, WEBP_FF_FRAME_COUNT);
    if (frame_count <= 0) {
        return 0;
    }

    indexed = 1;
    for (frame_index = 1; frame_index <= frame_count; ++frame_index) {
        if (!WebPDemuxGetFrame(demux, frame_index, &iter)) {
            indexed = 0;
            break;
        }

        if (!vp8l_payload_uses_color_indexing(iter.fragment.bytes,
                                              iter.fragment.size)) {
            indexed = 0;
            WebPDemuxReleaseIterator(&iter);
            break;
        }

        WebPDemuxReleaseIterator(&iter);
    }

    return indexed;
}


/*
 * Try to convert an RGB frame into PAL8 when palette mode is requested.
 *
 * This helper performs an exact-color scan only. It does not approximate
 * colors. If the frame contains more unique colors than the requested budget,
 * the original RGB pixels are preserved.
 */
static SIXELSTATUS
webp_parse_animation_start_frame_no(int *start_frame_no)
{
    SIXELSTATUS status;
    char const *env_value;
    char *endptr;
    long parsed;

    status = SIXEL_OK;
    env_value = NULL;
    endptr = NULL;
    parsed = 0;

    *start_frame_no = INT_MIN;
    env_value = sixel_compat_getenv("SIXEL_LOADER_ANIMATION_START_FRAME_NO");
    if (env_value == NULL || env_value[0] == '\0') {
        goto end;
    }

    parsed = strtol(env_value, &endptr, 10);
    if (endptr == env_value || *endptr != '\0') {
        sixel_helper_set_additional_message(
            "SIXEL_LOADER_ANIMATION_START_FRAME_NO must be an integer.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (parsed < (long)INT_MIN || parsed > (long)INT_MAX) {
        sixel_helper_set_additional_message(
            "SIXEL_LOADER_ANIMATION_START_FRAME_NO is out of range.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    *start_frame_no = (int)parsed;

end:
    return status;
}

static SIXELSTATUS
webp_resolve_animation_start_frame_no(int start_frame_no,
                                      int frame_count,
                                      int *resolved)
{
    SIXELSTATUS status;
    int index;

    status = SIXEL_OK;
    index = 0;

    if (frame_count <= 0) {
        sixel_helper_set_additional_message(
            "Animation frame count must be positive.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    if (start_frame_no >= 0) {
        index = start_frame_no;
    } else {
        index = frame_count + start_frame_no;
    }

    if (index < 0 || index >= frame_count) {
        sixel_helper_set_additional_message(
            "SIXEL_LOADER_ANIMATION_START_FRAME_NO is outside"
            " the animation frame range.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    *resolved = index;

end:
    return status;
}


static SIXELSTATUS
loader_try_promote_pal8(
    sixel_frame_t  /* in/out */ *frame,
    int            /* in */     reqcolors,
    sixel_allocator_t /* in */  *allocator)
{
    SIXELSTATUS status;
    unsigned char *src;
    unsigned char *indices;
    unsigned char *palette;
    int pixel_total;
    int maxcolors;
    int i;
    int j;
    int index;
    int ncolors;
    int offset;
    int found;
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned int key;
    unsigned int mixed;
    unsigned int step;
    unsigned int slot;
    unsigned int probe;
    unsigned int table_size;
    unsigned int table_mask;
    unsigned int *keys;
    unsigned char *values;
    int lookup_index;

#define PAL8_HASH_EMPTY_KEY 0xffffffffU

    status = SIXEL_OK;
    src = NULL;
    indices = NULL;
    palette = NULL;
    pixel_total = 0;
    maxcolors = 0;
    i = 0;
    j = 0;
    index = 0;
    ncolors = 0;
    offset = 0;
    found = 0;
    r = 0;
    g = 0;
    b = 0;
    key = 0U;
    mixed = 0U;
    step = 0U;
    slot = 0U;
    probe = 0U;
    table_size = 0U;
    table_mask = 0U;
    keys = NULL;
    values = NULL;
    lookup_index = 0;

    if (frame == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (frame->pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        return SIXEL_OK;
    }
    if (frame->width <= 0 || frame->height <= 0) {
        return SIXEL_BAD_INPUT;
    }

    pixel_total = frame->width * frame->height;
    if (pixel_total / frame->width != frame->height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    maxcolors = reqcolors;
    if (maxcolors <= 0 || maxcolors > SIXEL_PALETTE_MAX) {
        maxcolors = SIXEL_PALETTE_MAX;
    }

    src = frame->pixels.u8ptr;
    if (src == NULL) {
        return SIXEL_BAD_INPUT;
    }

    indices = (unsigned char *)sixel_allocator_malloc(allocator,
                                                      (size_t)pixel_total);
    if (indices == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    palette = (unsigned char *)sixel_allocator_malloc(allocator,
                                                      (size_t)maxcolors * 3U);
    if (palette == NULL) {
        sixel_allocator_free(allocator, indices);
        return SIXEL_BAD_ALLOCATION;
    }

    table_size = 2048U;
    while (table_size < (unsigned int)maxcolors * 4U) {
        table_size <<= 1U;
    }
    table_mask = table_size - 1U;

    keys = (unsigned int *)sixel_allocator_malloc(allocator,
                                                  sizeof(unsigned int) *
                                                  table_size);
    if (keys == NULL) {
        sixel_allocator_free(allocator, palette);
        sixel_allocator_free(allocator, indices);
        return SIXEL_BAD_ALLOCATION;
    }
    values = (unsigned char *)sixel_allocator_malloc(allocator,
                                                     table_size);
    if (values == NULL) {
        sixel_allocator_free(allocator, keys);
        sixel_allocator_free(allocator, palette);
        sixel_allocator_free(allocator, indices);
        return SIXEL_BAD_ALLOCATION;
    }

    for (i = 0; i < (int)table_size; ++i) {
        keys[i] = PAL8_HASH_EMPTY_KEY;
        values[i] = 0U;
    }

    for (i = 0; i < pixel_total; ++i) {
        offset = i * 3;
        r = src[offset + 0];
        g = src[offset + 1];
        b = src[offset + 2];
        key = ((unsigned int)r << 16)
            | ((unsigned int)g << 8)
            | (unsigned int)b;
        mixed = key;
        mixed ^= mixed >> 13;
        mixed *= 0x9e3779b1U;
        mixed ^= mixed >> 16;
        step = 0U;
        found = 0;
        index = 0;

        for (;;) {
            slot = (mixed + step) & table_mask;
            probe = keys[slot];
            if (probe == PAL8_HASH_EMPTY_KEY) {
                break;
            }
            if (probe == key) {
                lookup_index = (int)values[slot];
                if (lookup_index < ncolors &&
                    palette[lookup_index * 3 + 0] == r &&
                    palette[lookup_index * 3 + 1] == g &&
                    palette[lookup_index * 3 + 2] == b) {
                    found = 1;
                    index = lookup_index;
                    break;
                }
            }
            step++;
            if (step > table_mask) {
                break;
            }
        }

        if (!found && step > table_mask) {
            for (j = 0; j < ncolors; ++j) {
                if (palette[j * 3 + 0] == r &&
                    palette[j * 3 + 1] == g &&
                    palette[j * 3 + 2] == b) {
                    found = 1;
                    index = j;
                    break;
                }
            }
        }

        if (!found) {
            if (ncolors >= maxcolors) {
                sixel_allocator_free(allocator, values);
                sixel_allocator_free(allocator, keys);
                sixel_allocator_free(allocator, palette);
                sixel_allocator_free(allocator, indices);
                return SIXEL_OK;
            }
            index = ncolors;
            palette[index * 3 + 0] = r;
            palette[index * 3 + 1] = g;
            palette[index * 3 + 2] = b;
            ncolors++;

            if (step <= table_mask) {
                keys[slot] = key;
                values[slot] = (unsigned char)index;
            }
        }

        indices[i] = (unsigned char)index;
    }

    sixel_allocator_free(allocator, values);
    sixel_allocator_free(allocator, keys);

    sixel_allocator_free(allocator, frame->pixels.u8ptr);
    frame->pixels.u8ptr = NULL;

    sixel_frame_set_pixels(frame, indices);
    sixel_frame_set_palette(frame, palette);
    frame->ncolors = ncolors;
    frame->pixelformat = SIXEL_PIXELFORMAT_PAL8;
    frame->colorspace = SIXEL_COLORSPACE_GAMMA;

    return status;

#undef PAL8_HASH_EMPTY_KEY
}

static int
webp_frame_has_non_opaque_alpha(sixel_frame_t const *frame)
{
    unsigned char const *pixels;
    size_t pixel_count;
    size_t index;

    pixels = NULL;
    pixel_count = 0u;
    index = 0u;

    if (frame == NULL) {
        return 0;
    }
    if (frame->pixelformat != SIXEL_PIXELFORMAT_RGBA8888) {
        return 0;
    }
    if (frame->pixels.u8ptr == NULL || frame->width <= 0 || frame->height <= 0) {
        return 0;
    }
    if ((size_t)frame->width > SIZE_MAX / (size_t)frame->height) {
        return 0;
    }

    pixel_count = (size_t)frame->width * (size_t)frame->height;
    pixels = frame->pixels.u8ptr;
    for (index = 0u; index < pixel_count; ++index) {
        if (pixels[index * 4u + 3u] != 255u) {
            return 1;
        }
    }

    return 0;
}

static int
webp_should_preserve_alpha_keycolor(sixel_frame_t const *frame,
                                    unsigned char const *bgcolor)
{
    if (bgcolor != NULL) {
        return 0;
    }

    return webp_frame_has_non_opaque_alpha(frame);
}

static SIXELSTATUS
webp_finalize_frame_output(sixel_frame_t *frame,
                           int enable_cms,
                           int cms_converted,
                           int allow_palette_promotion,
                           int reqcolors,
                           unsigned char *bgcolor,
                           sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    int target_pixelformat;
    int apply_cms_target;
    int preserve_alpha_keycolor;

    status = SIXEL_FALSE;
    target_pixelformat = SIXEL_PIXELFORMAT_RGBFLOAT32;
    apply_cms_target = 0;
    preserve_alpha_keycolor = 0;
    if (frame == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    preserve_alpha_keycolor = webp_should_preserve_alpha_keycolor(frame,
                                                                  bgcolor);
    sixel_trace_topic_message(
        "webp_decode",
        "finalize frame_no=%d pixelformat=%d preserve_alpha_keycolor=%d "
        "has_bgcolor=%d allow_palette_promotion=%d",
        frame->frame_no,
        frame->pixelformat,
        preserve_alpha_keycolor,
        bgcolor != NULL ? 1 : 0,
        allow_palette_promotion != 0 ? 1 : 0);
    if (preserve_alpha_keycolor) {
        frame->transparent = -1;
        frame->alpha_zero_is_transparent = 1;
    } else {
        status = webp_blend_rgba_background_linear(frame, bgcolor, allocator);
        if (SIXEL_FAILED(status)) {
            return status;
        }

        status = sixel_frame_strip_alpha(frame, bgcolor);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        frame->transparent = -1;
        frame->alpha_zero_is_transparent = 0;
    }

    if (allow_palette_promotion && !preserve_alpha_keycolor) {
        status = loader_try_promote_pal8(frame, reqcolors, allocator);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }

    if (enable_cms && cms_converted) {
        target_pixelformat = loader_cms_target_pixelformat();
        apply_cms_target = 1;
    }

    if (frame->pixelformat == SIXEL_PIXELFORMAT_RGB888
            || frame->pixelformat == SIXEL_PIXELFORMAT_RGBFLOAT32
            || (apply_cms_target &&
                frame->pixelformat == SIXEL_PIXELFORMAT_LINEARRGBFLOAT32)) {
        status = sixel_frame_set_pixelformat(frame, target_pixelformat);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }

    return SIXEL_OK;
}

/*
 * Dedicated libwebp loader wiring minimal pipeline.
 *
 *    +------------+     +-------------------+     +--------------------+
 *    | WebP chunk | --> | libwebp decode    | --> | sixel frame emit   |
 *    +------------+     +-------------------+     +--------------------+
 */
static SIXELSTATUS
load_with_libwebp(
    sixel_chunk_t const       /* in */     *pchunk,
    int                       /* in */     enable_cms,
    int                       /* in */     fstatic,
    int                       /* in */     fuse_palette,
    int                       /* in */     reqcolors,
    unsigned char             /* in */     *bgcolor,
    int                       /* in */     loop_control,
    int                       /* in */     start_frame_no_set,
    int                       /* in */     start_frame_no_override,
    sixel_load_image_function /* in */     fn_load,
    void                      /* in/out */ *context)
{
    SIXELSTATUS status;
    sixel_frame_t *frame;
    unsigned char *pixels;
    WebPData webp_data;
    WebPAnimDecoderOptions decoder_options;
    WebPAnimDecoder *decoder;
    WebPAnimInfo anim_info;
    WebPDemuxer *demux;
    uint8_t *decoded_frame;
    int timestamp;
    int previous_timestamp;
    size_t frame_bytes;
    int next_delay;
    int frame_no;
    int loop_count;
    int allow_palette_promotion;
    int start_frame_no;
    int resolved_start_frame_no;
    int decode_start_frame_no;
    int emitted_frame_no;
    int cms_converted;
    WebPBitstreamFeatures stream_features;
    VP8StatusCode feature_status;
    unsigned int anim_bg_alpha;
    unsigned char anim_bgcolor[3];
    unsigned char *resolved_bgcolor;
    unsigned char *icc_profile;
    size_t icc_profile_length;
    size_t emitted_total_frames;
    char error_message[128];

    status = SIXEL_FALSE;
    frame = NULL;
    pixels = NULL;
    webp_data = (WebPData){ 0 };
    decoder = NULL;
    anim_info = (WebPAnimInfo){ 0 };
    demux = NULL;
    decoded_frame = NULL;
    timestamp = 0;
    previous_timestamp = 0;
    frame_bytes = 0;
    next_delay = 0;
    frame_no = 0;
    loop_count = 0;
    allow_palette_promotion = 0;
    start_frame_no = INT_MIN;
    resolved_start_frame_no = 0;
    decode_start_frame_no = 0;
    emitted_frame_no = 0;
    cms_converted = 0;
    stream_features = (WebPBitstreamFeatures){ 0 };
    feature_status = VP8_STATUS_OK;
    anim_bg_alpha = 0U;
    anim_bgcolor[0] = 0u;
    anim_bgcolor[1] = 0u;
    anim_bgcolor[2] = 0u;
    resolved_bgcolor = bgcolor;
    icc_profile = NULL;
    icc_profile_length = 0U;
    emitted_total_frames = 0u;
    memset(error_message, 0, sizeof(error_message));

    if (start_frame_no_set) {
        start_frame_no = start_frame_no_override;
    } else {
        status = webp_parse_animation_start_frame_no(&start_frame_no);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    webp_data.bytes = pchunk->buffer;
    webp_data.size = pchunk->size;

    status = webp_validate_riff_container(pchunk->buffer, pchunk->size);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    feature_status = WebPGetFeatures(pchunk->buffer,
                                     pchunk->size,
                                     &stream_features);
    if (feature_status != VP8_STATUS_OK) {
        (void)snprintf(error_message,
                       sizeof(error_message),
                       "load_with_libwebp: WebPGetFeatures failed (%s:%d).",
                       webp_decode_status_name(feature_status),
                       (int)feature_status);
        sixel_helper_set_additional_message(error_message);
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (stream_features.width > INT_MAX || stream_features.height > INT_MAX) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto end;
    }
    status = webp_validate_canvas_limits(stream_features.width,
                                         stream_features.height);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    if (enable_cms || fuse_palette) {
        demux = WebPDemux(&webp_data);
        if (demux == NULL) {
            sixel_helper_set_additional_message(
                "load_with_libwebp: WebPDemux failed.");
            status = SIXEL_BAD_INPUT;
            goto end;
        }
    }

    if (fuse_palette) {
        allow_palette_promotion = webp_input_is_indexed(demux);
    }

    if (enable_cms) {
        webp_extract_icc_profile(demux,
                                 &icc_profile,
                                 &icc_profile_length,
                                 pchunk->allocator);
    }

    if (!stream_features.has_animation) {
        if (start_frame_no != INT_MIN) {
            status = webp_resolve_animation_start_frame_no(start_frame_no,
                                                           1,
                                                           &resolved_start_frame_no);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
        }

        status = sixel_frame_new(&frame, pchunk->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        status = load_webp(&pixels,
                           pchunk->buffer,
                           pchunk->size,
                           &frame->width,
                           &frame->height,
                           &frame->pixelformat,
                           enable_cms,
                           icc_profile,
                           icc_profile_length,
                           &cms_converted,
                           bgcolor,
                           pchunk->allocator,
                           &stream_features);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        if (SIXEL_PIXELFORMAT_IS_FLOAT32(frame->pixelformat)) {
            sixel_frame_set_pixels_float32(frame, (float *)pixels);
        } else {
            sixel_frame_set_pixels(frame, pixels);
        }
        frame->frame_no = resolved_start_frame_no;

        status = webp_finalize_frame_output(frame,
                                            enable_cms,
                                            cms_converted,
                                            allow_palette_promotion,
                                            reqcolors,
                                            bgcolor,
                                            pchunk->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        status = fn_load(frame, context);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        status = SIXEL_OK;
        goto end;
    }

    if (!WebPAnimDecoderOptionsInit(&decoder_options)) {
        sixel_helper_set_additional_message(
            "load_with_libwebp: WebPAnimDecoderOptionsInit failed.");
        status = SIXEL_WEBP_ERROR;
        goto end;
    }
    decoder_options.color_mode = MODE_RGBA;
    decoder_options.use_threads = 1;
    decoder = WebPAnimDecoderNew(&webp_data, &decoder_options);
    if (decoder == NULL) {
        sixel_helper_set_additional_message(
            "load_with_libwebp: WebPAnimDecoderNew failed.");
        status = SIXEL_WEBP_ERROR;
        goto end;
    }

    if (!WebPAnimDecoderGetInfo(decoder, &anim_info)) {
        sixel_helper_set_additional_message(
            "load_with_libwebp: WebPAnimDecoderGetInfo failed.");
        status = SIXEL_WEBP_ERROR;
        goto end;
    }
    if (anim_info.canvas_width > (unsigned int)INT_MAX ||
        anim_info.canvas_height > (unsigned int)INT_MAX) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto end;
    }
    status = webp_validate_canvas_limits((int)anim_info.canvas_width,
                                         (int)anim_info.canvas_height);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (anim_info.frame_count > WEBP_MAX_ANIMATION_FRAMES) {
        sixel_helper_set_additional_message(
            "load_with_libwebp: animation frame count exceeds limit.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    if (bgcolor == NULL && stream_features.has_alpha) {
        anim_bg_alpha = (anim_info.bgcolor >> 24u) & 0xffu;
        /*
         * WebPAnimInfo::bgcolor stores ANIM background as
         * 0xAARRGGBB. Extract RGB in that order.
         */
        anim_bgcolor[0] = (unsigned char)((anim_info.bgcolor >> 16u) & 0xffu);
        anim_bgcolor[1] = (unsigned char)((anim_info.bgcolor >> 8u) & 0xffu);
        anim_bgcolor[2] = (unsigned char)(anim_info.bgcolor & 0xffu);
        if (anim_bg_alpha == 255u) {
            resolved_bgcolor = anim_bgcolor;
            sixel_trace_topic_message(
                "webp_decode",
                "animation background source=ANIM alpha=255 rgb=#%02x%02x%02x",
                anim_bgcolor[0],
                anim_bgcolor[1],
                anim_bgcolor[2]);
        } else if (anim_bg_alpha == 0u) {
            sixel_trace_topic_message(
                "webp_decode",
                "animation background source=ANIM alpha=0; keep transparent path");
        } else {
            sixel_trace_topic_message(
                "webp_decode",
                "animation background source=ANIM alpha=%u (non-opaque); keep transparent path rgb=#%02x%02x%02x",
                anim_bg_alpha,
                anim_bgcolor[0],
                anim_bgcolor[1],
                anim_bgcolor[2]);
        }
    }
    if (bgcolor != NULL) {
        sixel_trace_topic_message(
            "webp_decode",
            "animation background source=explicit rgb=#%02x%02x%02x",
            bgcolor[0],
            bgcolor[1],
            bgcolor[2]);
    } else if (resolved_bgcolor == NULL) {
        sixel_trace_topic_message(
            "webp_decode",
            "animation background source=none");
    }

    if (anim_info.frame_count <= 1) {
        if (start_frame_no != INT_MIN) {
            status = webp_resolve_animation_start_frame_no(start_frame_no,
                            anim_info.frame_count,
                            &resolved_start_frame_no);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
        }
        WebPAnimDecoderDelete(decoder);
        decoder = NULL;

        status = sixel_frame_new(&frame, pchunk->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        status = load_webp(&pixels,
                           pchunk->buffer,
                           pchunk->size,
                           &frame->width,
                           &frame->height,
                           &frame->pixelformat,
                           enable_cms,
                           icc_profile,
                           icc_profile_length,
                           &cms_converted,
                           resolved_bgcolor,
                           pchunk->allocator,
                           &stream_features);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        if (SIXEL_PIXELFORMAT_IS_FLOAT32(frame->pixelformat)) {
            sixel_frame_set_pixels_float32(frame, (float *)pixels);
        } else {
            sixel_frame_set_pixels(frame, pixels);
        }
        status = webp_finalize_frame_output(frame,
                                            enable_cms,
                                            cms_converted,
                                            allow_palette_promotion,
                                            reqcolors,
                                            resolved_bgcolor,
                                            pchunk->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        status = fn_load(frame, context);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        status = SIXEL_OK;
        goto end;
    }

    if (fstatic) {
        if (start_frame_no != INT_MIN) {
            status = webp_resolve_animation_start_frame_no(start_frame_no,
                            anim_info.frame_count,
                            &resolved_start_frame_no);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
        }

        status = sixel_frame_new(&frame, pchunk->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        previous_timestamp = 0;
        next_delay = 0;
        for (frame_no = 0; frame_no <= resolved_start_frame_no; frame_no++) {
            if (!WebPAnimDecoderHasMoreFrames(decoder)) {
                sixel_helper_set_additional_message(
                    "load_with_libwebp: no frames in animated WebP stream.");
                status = SIXEL_BAD_INPUT;
                goto end;
            }
            if (!WebPAnimDecoderGetNext(decoder, &decoded_frame, &timestamp)) {
                sixel_helper_set_additional_message(
                    "load_with_libwebp: WebPAnimDecoderGetNext failed.");
                status = SIXEL_WEBP_ERROR;
                goto end;
            }
            next_delay = timestamp - previous_timestamp;
            if (next_delay < 0) {
                next_delay = 0;
            }
            previous_timestamp = timestamp;
        }

        frame->width = (int)anim_info.canvas_width;
        frame->height = (int)anim_info.canvas_height;
        frame->pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
        frame->colorspace = SIXEL_COLORSPACE_GAMMA;
        frame->multiframe = 0;
        frame->loop_count = 0;
        frame->frame_no = resolved_start_frame_no;
        frame->delay = next_delay / 10;

        if ((size_t)frame->width > SIZE_MAX / 4 ||
            (size_t)frame->height > SIZE_MAX / ((size_t)frame->width * 4)) {
            status = SIXEL_BAD_INTEGER_OVERFLOW;
            goto end;
        }
        frame_bytes = (size_t)frame->width * (size_t)frame->height * 4;

        pixels = (unsigned char *)sixel_allocator_malloc(pchunk->allocator,
                                                         frame_bytes);
        if (pixels == NULL) {
            sixel_helper_set_additional_message(
                "load_with_libwebp: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        memcpy(pixels, decoded_frame, frame_bytes);
        cms_converted = 0;
        if (enable_cms) {
            cms_converted = webp_convert_embedded_icc_to_srgb(
                pixels,
                frame->width,
                frame->height,
                frame->pixelformat,
                icc_profile,
                icc_profile_length,
                pchunk->allocator);
        }
        sixel_frame_set_pixels(frame, pixels);
        status = webp_finalize_frame_output(frame,
                                            enable_cms,
                                            cms_converted,
                                            allow_palette_promotion,
                                            reqcolors,
                                            resolved_bgcolor,
                                            pchunk->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        status = fn_load(frame, context);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        status = SIXEL_OK;
        goto end;
    }

    /*
     * Decode WebP animation as fully composited RGBA canvases.
     *
     *   outer loop : logical animation loop
     *   inner loop : frame traversal inside a single loop
     *
     * Create a fresh sixel_frame_t for each callback invocation. This keeps
     * frame state isolated and avoids leaking in-place updates from one frame
     * into the next frame.
     */
    if ((int)anim_info.canvas_width <= 0 ||
        (int)anim_info.canvas_height <= 0) {
        sixel_helper_set_additional_message(
            "load_with_libwebp: invalid canvas dimensions.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    frame_bytes = (size_t)anim_info.canvas_width *
                  (size_t)anim_info.canvas_height;
    if ((size_t)anim_info.canvas_width != 0 &&
        frame_bytes / (size_t)anim_info.canvas_width !=
        (size_t)anim_info.canvas_height) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto end;
    }
    if (frame_bytes > SIZE_MAX / 4) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto end;
    }
    frame_bytes *= 4;

    if (start_frame_no != INT_MIN) {
        status = webp_resolve_animation_start_frame_no(start_frame_no,
                        anim_info.frame_count,
                        &resolved_start_frame_no);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    for (;;) {
        decode_start_frame_no = 0;
        if (loop_count == 0 && start_frame_no != INT_MIN) {
            decode_start_frame_no = resolved_start_frame_no;
        }

        frame_no = 0;
        emitted_frame_no = 0;
        previous_timestamp = 0;

        while (WebPAnimDecoderHasMoreFrames(decoder)) {
            if (!WebPAnimDecoderGetNext(decoder,
                                        &decoded_frame,
                                        &timestamp)) {
                sixel_helper_set_additional_message(
                    "load_with_libwebp: WebPAnimDecoderGetNext failed.");
                status = SIXEL_WEBP_ERROR;
                goto end;
            }

            if (frame_no < decode_start_frame_no) {
                previous_timestamp = timestamp;
                frame_no++;
                continue;
            }

            if (emitted_total_frames >= WEBP_MAX_OUTPUT_FRAMES) {
                sixel_helper_set_additional_message(
                    "load_with_libwebp: emitted frame count exceeds safety limit.");
                status = SIXEL_BAD_INPUT;
                goto end;
            }

            status = sixel_frame_new(&frame, pchunk->allocator);
            if (SIXEL_FAILED(status)) {
                goto end;
            }

            pixels = (unsigned char *)sixel_allocator_malloc(
                pchunk->allocator,
                frame_bytes);
            if (pixels == NULL) {
                sixel_helper_set_additional_message(
                    "load_with_libwebp: sixel_allocator_malloc() failed.");
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }

            memcpy(pixels, decoded_frame, frame_bytes);
            cms_converted = 0;
            if (enable_cms) {
                cms_converted = webp_convert_embedded_icc_to_srgb(
                    pixels,
                    (int)anim_info.canvas_width,
                    (int)anim_info.canvas_height,
                    SIXEL_PIXELFORMAT_RGBA8888,
                    icc_profile,
                    icc_profile_length,
                    pchunk->allocator);
            }
            sixel_frame_set_pixels(frame, pixels);
            pixels = NULL;

            frame->width = (int)anim_info.canvas_width;
            frame->height = (int)anim_info.canvas_height;
            frame->pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
            frame->colorspace = SIXEL_COLORSPACE_GAMMA;
            frame->multiframe = 1;
            frame->loop_count = loop_count;
            frame->frame_no = emitted_frame_no;

            next_delay = timestamp - previous_timestamp;
            if (next_delay < 0) {
                next_delay = 0;
            }
            frame->delay = next_delay / 10;

            status = webp_finalize_frame_output(frame,
                                                enable_cms,
                                                cms_converted,
                                                allow_palette_promotion,
                                                reqcolors,
                                                resolved_bgcolor,
                                                pchunk->allocator);
            if (SIXEL_FAILED(status)) {
                goto end;
            }

            status = fn_load(frame, context);
            if (status == SIXEL_INTERRUPTED) {
                goto end;
            }
            if (SIXEL_FAILED(status)) {
                goto end;
            }

            sixel_frame_unref(frame);
            frame = NULL;

            emitted_total_frames++;
            previous_timestamp = timestamp;
            emitted_frame_no++;
            frame_no++;
        }

        loop_count++;

        if (loop_control == SIXEL_LOOP_DISABLE || emitted_frame_no == 1) {
            break;
        }
        if (loop_control == SIXEL_LOOP_AUTO &&
            anim_info.loop_count > 0 &&
            (unsigned int)loop_count >= anim_info.loop_count) {
            break;
        }

        WebPAnimDecoderReset(decoder);
    }

    status = SIXEL_OK;

end:
    if (decoder != NULL) {
        WebPAnimDecoderDelete(decoder);
    }
    if (demux != NULL) {
        WebPDemuxDelete(demux);
    }
    sixel_allocator_free(pchunk->allocator, icc_profile);
    sixel_frame_unref(frame);

    return status;
}


static void
sixel_loader_libwebp_ref(sixel_loader_component_t *component)
{
    sixel_loader_libwebp_component_t *self;

    self = NULL;
    if (component == NULL) {
        return;
    }

    self = (sixel_loader_libwebp_component_t *)component;
    ++self->ref;
}

static void
sixel_loader_libwebp_unref(sixel_loader_component_t *component)
{
    sixel_loader_libwebp_component_t *self;
    sixel_allocator_t *allocator;

    self = NULL;
    allocator = NULL;
    if (component == NULL) {
        return;
    }

    self = (sixel_loader_libwebp_component_t *)component;
    if (self->ref == 0u) {
        return;
    }

    --self->ref;
    if (self->ref > 0u) {
        return;
    }

    allocator = self->allocator;
    sixel_allocator_free(allocator, self);
    sixel_allocator_unref(allocator);
}

static SIXELSTATUS
sixel_loader_libwebp_setopt(sixel_loader_component_t *component,
                            int option,
                            void const *value)
{
    sixel_loader_libwebp_component_t *self;
    int const *flag;
    unsigned char const *color;

    self = NULL;
    flag = NULL;
    color = NULL;
    if (component == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    self = (sixel_loader_libwebp_component_t *)component;
    switch (option) {
    case SIXEL_LOADER_COMPONENT_OPTION_LIBWEBP_ENABLE_CMS:
        flag = (int const *)value;
        self->enable_cms = (flag != NULL && *flag != 0) ? 1 : 0;
        return SIXEL_OK;
    case SIXEL_LOADER_COMPONENT_OPTION_CMS_ENGINE:
        flag = (int const *)value;
        if (flag != NULL && *flag >= 0) {
            self->enable_cms = (*flag == SIXEL_CMS_ENGINE_NONE) ? 0 : 1;
        }
        sixel_helper_set_loader_cms_engine(flag != NULL ? *flag : -1);
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_REQUIRE_STATIC:
        flag = (int const *)value;
        self->fstatic = flag != NULL ? *flag : 0;
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_USE_PALETTE:
        flag = (int const *)value;
        self->fuse_palette = flag != NULL ? *flag : 0;
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_REQCOLORS:
        flag = (int const *)value;
        if (flag != NULL) {
            self->reqcolors = *flag;
        }
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_BGCOLOR:
        if (value == NULL) {
            self->has_bgcolor = 0;
            return SIXEL_OK;
        }
        color = (unsigned char const *)value;
        self->bgcolor[0] = color[0];
        self->bgcolor[1] = color[1];
        self->bgcolor[2] = color[2];
        self->has_bgcolor = 1;
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_LOOP_CONTROL:
        flag = (int const *)value;
        if (flag != NULL) {
            self->loop_control = *flag;
        }
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_START_FRAME_NO:
        if (value == NULL) {
            self->has_start_frame_no = 0;
            self->start_frame_no = INT_MIN;
            return SIXEL_OK;
        }
        flag = (int const *)value;
        self->start_frame_no = *flag;
        self->has_start_frame_no = 1;
        return SIXEL_OK;
    default:
        return SIXEL_OK;
    }
}

static SIXELSTATUS
sixel_loader_libwebp_load(sixel_loader_component_t *component,
                          sixel_chunk_t const *chunk,
                          sixel_load_image_function fn_load,
                          void *context)
{
    sixel_loader_libwebp_component_t *self;
    unsigned char *bgcolor;

    self = NULL;
    bgcolor = NULL;
    if (component == NULL || chunk == NULL || fn_load == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    self = (sixel_loader_libwebp_component_t *)component;
    if (self->has_bgcolor) {
        bgcolor = self->bgcolor;
    }

    return load_with_libwebp(chunk,
                             self->enable_cms,
                             self->fstatic,
                             self->fuse_palette,
                             self->reqcolors,
                             bgcolor,
                             self->loop_control,
                             self->has_start_frame_no,
                             self->start_frame_no,
                             fn_load,
                             context);
}

static char const *
sixel_loader_libwebp_name(sixel_loader_component_t const *component)
{
    (void)component;
    return "libwebp";
}

static sixel_loader_component_vtbl_t const g_sixel_loader_libwebp_vtbl = {
    sixel_loader_libwebp_ref,
    sixel_loader_libwebp_unref,
    sixel_loader_libwebp_setopt,
    sixel_loader_libwebp_load,
    sixel_loader_libwebp_name
};

SIXELSTATUS
sixel_loader_libwebp_new(sixel_allocator_t *allocator,
                         sixel_loader_component_t **ppcomponent)
{
    sixel_loader_libwebp_component_t *self;

    self = NULL;
    if (allocator == NULL || ppcomponent == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *ppcomponent = NULL;
    self = (sixel_loader_libwebp_component_t *)
        sixel_allocator_malloc(allocator, sizeof(*self));
    if (self == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    memset(self, 0, sizeof(*self));
    self->base.vtbl = &g_sixel_loader_libwebp_vtbl;
    self->allocator = allocator;
    self->ref = 1u;
    self->enable_cms = 0;
    self->reqcolors = 256;
    self->start_frame_no = INT_MIN;
    sixel_allocator_ref(allocator);
    *ppcomponent = &self->base;
    return SIXEL_OK;
}

int
loader_can_try_libwebp(sixel_chunk_t const *chunk)
{
    if (chunk == NULL) {
        return 0;
    }

    return chunk_is_webp(chunk);
}

#else  /* !HAVE_WEBP */

/*
 * Keep a harmless placeholder around so pedantic builds skip the empty unit
 * warning when libwebp is not part of the build.
 */
enum { sixel_loader_libwebp_placeholder = 0 };

#if defined(__GNUC__) || defined(__clang__)
# define SIXEL_LIBWEBP_PLACEHOLDER_UNUSED __attribute__((unused))
#else
# define SIXEL_LIBWEBP_PLACEHOLDER_UNUSED
#endif

static void
sixel_loader_libwebp_placeholder_function(void)
    SIXEL_LIBWEBP_PLACEHOLDER_UNUSED;

static void
sixel_loader_libwebp_placeholder_function(void)
{
    /*
     * The placeholder ties the enum to a symbol so MSVC does not warn about
     * an empty translation unit when libwebp support is disabled.
     */
    (void)sixel_loader_libwebp_placeholder;
}

#undef SIXEL_LIBWEBP_PLACEHOLDER_UNUSED

#endif  /* HAVE_WEBP */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
