/* SPDX-License-Identifier: MIT */
/* Observe the actual adapter under isolated names. Original strip samples
 * have a literal oracle; successful colors use the existing byte CMM as an
 * adapter reference, not as independent evidence of CMM numeric accuracy. */
#include "config.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/cms.h"
#include "src/frame-private.h"
#include "src/loader-libtiff.h"
#include "src/loader-common.h"
#include "src/chunk-factory.h"
#include "src/chunk-view.h"
#include "tiff_legacy_test_common.h"

#if HAVE_LIBTIFF
#include <tiffio.h>
#include "tests/data/color-numeric/tiff-legacy.h"

static struct {
    int fixture, engine, mode, armed, managed, baseline;
    int ok, calls, reads, strips, endpoints, api_calls, creates;
    int allocations, profiles, transforms, attempts, injected;
    int real_pixels, changed, poisoned, phases;
    int base_alpha;
    size_t n, profile_size, base_mask_size;
    unsigned char raw[64], expected[64], profile[4096], base_mask[16];
    void *heap[2];
    sixel_cms_profile_t *handles[2];
    sixel_cms_transform_t *transform;
} lc;

static void
lc_begin(int applies)
{
    ++lc.phases;
    fprintf(stderr, "LC1|begin|engine=%s|applies=%d\n",
        sixel_cms_engine_to_string((sixel_cms_engine_t)lc.engine), applies);
}

static void
lc_end(int ok)
{
    fprintf(stderr, "LC1|end|ok=%d\n", ok);
}

static void *
lc_malloc(size_t bytes)
{
    void *data;
    size_t expected;
    int i;

    if (!lc.armed) { return malloc(bytes); }
    ++lc.attempts;
#if HAVE_LCMS2
    expected = tl_cases[lc.fixture].photo == 1
        ? lc.n * (lc.attempts == 1 ? 1u : 3u) : lc.n * 4u;
#else
    expected = lc.n * 3u;
#endif
    lc.ok &= bytes == expected;
    if ((lc.mode == 3 && lc.attempts == 1) ||
        (lc.mode == 4 && lc.attempts == 2)) {
        ++lc.injected;
        return NULL;
    }
    data = malloc(bytes);
    if (data != NULL) {
        ++lc.allocations;
        /* The marker qualifies a Gray scratch-copy mutation. It is never
         * claimed to be ordinary uninitialized allocator contents. */
        memset(data, 0xa5, bytes);
        for (i = 0; i < 2 && lc.heap[i] != NULL; ++i) { }
        if (i == 2) { lc.ok = 0; } else { lc.heap[i] = data; }
    }
    return data;
}

static void
lc_free(void *data)
{
    int i;

    if (lc.armed && data != NULL) {
        for (i = 0; i < 2 && lc.heap[i] != data; ++i) { }
        if (i == 2) { lc.ok = 0; } else { lc.heap[i] = NULL; }
        --lc.allocations;
    }
    free(data);
}

static sixel_cms_profile_t *
lc_track_profile(sixel_cms_profile_t *profile)
{
    int i;

    if (lc.armed && profile != NULL) {
        ++lc.profiles;
        for (i = 0; i < 2 && lc.handles[i] != NULL; ++i) { }
        if (i == 2) { lc.ok = 0; } else { lc.handles[i] = profile; }
    }
    return profile;
}

static sixel_cms_profile_t *
lc_open(void const *data, size_t bytes)
{
    return lc_track_profile(sixel_cms_open_profile_from_mem(data, bytes));
}

static void
lc_close(sixel_cms_profile_t *profile)
{
    int i;

    if (lc.armed && profile != NULL) {
        for (i = 0; i < 2 && lc.handles[i] != profile; ++i) { }
        if (i == 2) { lc.ok = 0; } else { lc.handles[i] = NULL; }
        --lc.profiles;
    }
    sixel_cms_close_profile(profile);
}

/* Qualify the real changed prefix against the separately computed ordinary
 * CMM result. This proves useful work happened before the false return. */
static void
lc_prefix(unsigned char const *out, size_t channels)
{
    size_t i;
    size_t channel;

    for (i = 0u; i < 2u; ++i) {
        for (channel = 0u; channel < 3u; ++channel) {
            lc.ok &= out[i * channels + channel] ==
                     lc.expected[i * 4u + channel];
            lc.changed |= out[i * channels + channel] !=
                          lc.raw[i * 4u + channel];
        }
    }
}

#if HAVE_LCMS2
static sixel_cms_profile_t *
lc_srgb(void)
{
    return lc_track_profile(sixel_cms_create_srgb_profile());
}

static sixel_cms_transform_t *
lc_create(sixel_cms_profile_t const *source,
          sixel_cms_pixel_format_t input,
          sixel_cms_profile_t const *target,
          sixel_cms_pixel_format_t output, int flags)
{
    sixel_cms_transform_t *transform;
    int gray;

    ++lc.creates;
    gray = tl_cases[lc.fixture].photo == 1;
    lc.ok &= input == (gray ? SIXEL_CMS_PIXELFORMAT_GRAY_8 :
                       SIXEL_CMS_PIXELFORMAT_RGBA_8);
    lc.ok &= output == (gray ? SIXEL_CMS_PIXELFORMAT_RGB_8 :
                        SIXEL_CMS_PIXELFORMAT_RGBA_8);
    lc.ok &= flags == (gray ? SIXEL_CMS_TRANSFORM_DEFAULT :
                       SIXEL_CMS_TRANSFORM_COPY_ALPHA);
    if (lc.mode == 5) { ++lc.injected; return NULL; }
    transform = sixel_cms_create_transform(source, input, target, output,
                                           flags);
    if (transform != NULL) {
        ++lc.transforms;
        lc.transform = transform;
        lc.ok &= (int)sixel_cms_transform_engine(transform) == lc.engine;
    }
    return transform;
}

static void
lc_delete(sixel_cms_transform_t *transform)
{
    if (transform != NULL) {
        lc.ok &= transform == lc.transform;
        lc.transform = NULL;
        --lc.transforms;
    }
    sixel_cms_delete_transform(transform);
}

static int
lc_apply(sixel_cms_transform_t const *transform, void const *source,
         void *target, size_t count)
{
    unsigned char const *input;
    unsigned char *output;
    size_t i;
    size_t n;
    int gray;
    int ok;

    ++lc.api_calls;
    input = source;
    output = target;
    gray = tl_cases[lc.fixture].photo == 1;
    lc.ok &= count == lc.n;
    if (gray) {
        for (i = 0u; i < count; ++i) {
            lc.ok &= input[i] == lc.raw[i * 4u];
        }
    } else {
        lc.ok &= source == target;
        lc.ok &= memcmp(input, lc.raw, count * 4u) == 0;
    }
    n = lc.mode == 1 ? 2u : count;
    ok = sixel_cms_do_transform(transform, source, target, n);
    lc.ok &= ok != 0;
    lc.real_pixels += ok ? (int)n : 0;
    if (ok && lc.mode == 1) { lc_prefix(output, gray ? 3u : 4u); }
    if (ok && lc.mode == 6 && !gray) {
        for (i = 0u; i < count; ++i) { output[i * 4u + 3u] ^= 0xffu; }
        lc.poisoned += (int)count;
    }
    if (lc.mode == 1) { ++lc.injected; return 0; }
    return ok;
}
#else
static int
lc_convert(unsigned char *pixels, int width, int height, int format,
           unsigned char const *profile, size_t length)
{
    size_t i;
    size_t channel;
    int ok;

    ++lc.api_calls;
    lc.ok &= format == SIXEL_PIXELFORMAT_RGB888;
    lc.ok &= (size_t)width * (size_t)height == lc.n;
    for (i = 0u; i < lc.n; ++i) {
        for (channel = 0u; channel < 3u; ++channel) {
            lc.ok &= pixels[i * 3u + channel] == lc.raw[i * 4u + channel];
        }
    }
    ok = sixel_cms_convert_to_srgb_with_profile_bytes(pixels,
        lc.mode == 1 ? 2 : width, lc.mode == 1 ? 1 : height,
        format, profile, length);
    lc.ok &= ok != 0;
    lc.real_pixels += ok ? (lc.mode == 1 ? 2 : (int)lc.n) : 0;
    if (ok && lc.mode == 1) { lc_prefix(pixels, 3u); }
    if (lc.mode == 1) { ++lc.injected; return 0; }
    return ok;
}
#endif

/* Inspect original decoded strips after the RGBA read has completed. No
 * directory reset or writer is used, and the borrowed ICC tag stays alive. */
static int
lc_rgba(TIFF *tif, uint32_t width, uint32_t height, uint32_t *raster,
        int orientation, int stop)
{
    uint16_t words[64];
    unsigned char *bytes;
    void *profile;
    uint32_t length;
    uint16_t *red;
    uint16_t *green;
    uint16_t *blue;
    uint16_t photo;
    uint16_t bits;
    uint16_t planar;
    uint16_t channels;
    size_t count;
    size_t i;
    size_t index;
    size_t row_samples;
    size_t row_bytes;
    size_t bit;
    size_t expected_bytes;
    unsigned int value;
    int plane;
    int planes;
    int ok;

    ++lc.reads;
    ok = TIFFReadRGBAImageOriented(tif, width, height, raster,
                                   orientation, stop);
    if (!ok) { return ok; }
    lc.ok &= width == (uint32_t)tl_cases[lc.fixture].width &&
             height == (uint32_t)tl_cases[lc.fixture].height;
    TIFFGetFieldDefaulted(tif, TIFFTAG_PHOTOMETRIC, &photo);
    TIFFGetFieldDefaulted(tif, TIFFTAG_BITSPERSAMPLE, &bits);
    TIFFGetFieldDefaulted(tif, TIFFTAG_PLANARCONFIG, &planar);
    TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLESPERPIXEL, &channels);
    lc.ok &= photo == tl_cases[lc.fixture].photo &&
             bits == tl_cases[lc.fixture].bits &&
             planar == tl_cases[lc.fixture].planar &&
             channels == tl_cases[lc.fixture].channels;
    for (i = 0u; i < lc.n; ++i) {
        lc.raw[i * 4u] = TIFFGetR(raster[i]);
        lc.raw[i * 4u + 1u] = TIFFGetG(raster[i]);
        lc.raw[i * 4u + 2u] = TIFFGetB(raster[i]);
        lc.raw[i * 4u + 3u] = TIFFGetA(raster[i]);
    }
    bytes = (unsigned char *)words;
    planes = planar == PLANARCONFIG_SEPARATE ? channels : 1;
    count = lc.n * (planar == PLANARCONFIG_SEPARATE ? 1u : channels);
    row_samples = (size_t)width *
        (planar == PLANARCONFIG_SEPARATE ? 1u : channels);
    row_bytes = (row_samples * bits + 7u) / 8u;
    expected_bytes = row_bytes * height;
    lc.ok &= expected_bytes <= sizeof(words) &&
             TIFFNumberOfStrips(tif) == (uint32_t)planes;
    for (plane = 0; plane < planes && lc.ok; ++plane) {
        lc.ok &= TIFFReadEncodedStrip(tif, (uint32_t)plane, words,
                                      sizeof(words)) ==
                 (tmsize_t)expected_bytes;
        if (!lc.ok) { return ok; }
        for (i = 0u; i < count; ++i) {
            /* Packed low-bit rows end on a byte boundary independently. */
            bit = (i / row_samples) * row_bytes * 8u +
                (i % row_samples) * bits;
            value = bits == 16 ? words[i] : bits == 8 ? bytes[i] :
                (bytes[bit / 8u] >> (8u - bits - bit % 8u)) & 15u;
            index = planar == PLANARCONFIG_SEPARATE
                ? i * channels + (size_t)plane : i;
            lc.ok &= value == tl_cases[lc.fixture].source[index];
        }
        ++lc.strips;
    }
    if (photo == PHOTOMETRIC_PALETTE) {
        lc.ok &= TIFFGetField(tif, TIFFTAG_COLORMAP, &red, &green, &blue);
        for (i = 0u; i < 16u && lc.ok; ++i) {
            lc.ok &= red[i] == tl_cases[lc.fixture].colormap[i * 3u] &&
                green[i] == tl_cases[lc.fixture].colormap[i * 3u + 1u] &&
                blue[i] == tl_cases[lc.fixture].colormap[i * 3u + 2u];
        }
    }
    profile = NULL;
    length = 0u;
    lc.ok &= TIFFGetField(tif, TIFFTAG_ICCPROFILE, &length, &profile) &&
             length <= sizeof(lc.profile);
    if (lc.ok) {
        memcpy(lc.profile, profile, length);
        lc.profile_size = length;
    }
    return ok;
}

/* This is the actual helper result before the existing public alpha strip.
 * Protect original RGBA alpha even when later processing hides the error. */
static SIXELSTATUS
lc_strip(sixel_frame_t *frame, unsigned char *background)
{
    unsigned char const *pixels;
    size_t i;
    size_t channel;

    ++lc.endpoints;
    pixels = sixel_frame_get_pixels(frame);
    lc.ok &= sixel_frame_get_pixelformat(frame) == SIXEL_PIXELFORMAT_RGBA8888;
    for (i = 0u; i < lc.n && lc.ok; ++i) {
        for (channel = 0u; channel < 4u; ++channel) {
            lc.ok &= pixels[i * 4u + channel] ==
                (lc.managed && channel < 3u ? lc.expected : lc.raw)
                [i * 4u + channel];
        }
    }
    return sixel_frame_strip_alpha(frame, background);
}

#define g_sixel_loader_libtiff_vtbl lc_local_0
#define load_tiff lc_local_1
#define load_with_libtiff lc_local_2
#define loader_can_try_libtiff lc_local_3
#define sixel_loader_libtiff_component lc_local_4
#define sixel_loader_libtiff_component_t lc_local_5
#define sixel_loader_libtiff_load lc_local_6
#define sixel_loader_libtiff_name lc_local_7
#define sixel_loader_libtiff_new lc_local_8
#define sixel_loader_libtiff_placeholder lc_local_9
#define sixel_loader_libtiff_placeholder_function lc_local_10
#define sixel_loader_libtiff_predicate lc_local_11
#define sixel_loader_libtiff_ref lc_local_12
#define sixel_loader_libtiff_setopt lc_local_13
#define sixel_loader_libtiff_unref lc_local_14
#define tiff_clamp_lab_ab lc_local_15
#define tiff_clamp_unit lc_local_16
#define tiff_convert_embedded_icc_to_srgb lc_local_17
#define tiff_convert_embedded_icc_to_srgb_nolcms lc_local_19
#define tiff_decode_lab_u16_ab lc_local_21
#define tiff_decode_lab_u16_l lc_local_22
#define tiff_map_scanline_coordinates lc_local_23
#define tiff_memory_chunk lc_local_24
#define tiff_memory_chunk_t lc_local_25
#define tiff_memory_close lc_local_26
#define tiff_memory_map lc_local_27
#define tiff_memory_read lc_local_28
#define tiff_memory_seek lc_local_29
#define tiff_memory_size lc_local_30
#define tiff_memory_unmap lc_local_31
#define tiff_memory_write lc_local_32
#define tiff_normalize_lab_ab_sample lc_local_33
#define tiff_normalize_lab_l_sample lc_local_34
#define tiff_orientation_is_supported lc_local_35
#define tiff_photometric_allows_embedded_icc lc_local_36
#define tiff_try_load_high_precision lc_local_37
#define tiff_logluv_xyz_to_linear lc_local_38
#define tiff_convert_samples_to_linear lc_local_39
#define tiff_try_load_integer_alpha lc_local_40
#define malloc lc_malloc
#define free lc_free
#define sixel_cms_open_profile_from_mem lc_open
#define sixel_cms_close_profile lc_close
#define TIFFReadRGBAImageOriented lc_rgba
#define sixel_frame_strip_alpha lc_strip
#if HAVE_LCMS2
#define sixel_cms_create_srgb_profile lc_srgb
#define sixel_cms_create_transform lc_create
#define sixel_cms_delete_transform lc_delete
#define sixel_cms_do_transform lc_apply
#else
#define sixel_cms_convert_to_srgb_with_profile_bytes lc_convert
#endif
#define tiff_read_separate_uint16 b1_namespace_1
#define tiff_publish_integer_alpha til_ns_0_0
#define tiff_integer_layout_t til_ns_0_1
#define tiff_scatter_integer_row til_ns_0_2
#define tiff_try_load_integer_layouts til_ns_0_3
#define tiff_lab8_ranges tlx_copy_5_ranges
#define tiff_lab8_to_linear tlx_copy_5_linear
#include "src/loader-libtiff.c"
#undef tiff_lab8_to_linear
#undef tiff_lab8_ranges
#undef tiff_publish_integer_alpha
#undef tiff_integer_layout_t
#undef tiff_scatter_integer_row
#undef tiff_try_load_integer_layouts

#undef tiff_read_separate_uint16
#undef g_sixel_loader_libtiff_vtbl
#undef load_tiff
#undef load_with_libtiff
#undef loader_can_try_libtiff
#undef sixel_loader_libtiff_component
#undef sixel_loader_libtiff_component_t
#undef sixel_loader_libtiff_load
#undef sixel_loader_libtiff_name
#undef sixel_loader_libtiff_new
#undef sixel_loader_libtiff_placeholder
#undef sixel_loader_libtiff_placeholder_function
#undef sixel_loader_libtiff_predicate
#undef sixel_loader_libtiff_ref
#undef sixel_loader_libtiff_setopt
#undef sixel_loader_libtiff_unref
#undef tiff_clamp_lab_ab
#undef tiff_clamp_unit
#undef tiff_convert_embedded_icc_to_srgb
#undef tiff_convert_embedded_icc_to_srgb_nolcms
#undef tiff_decode_lab_u16_ab
#undef tiff_decode_lab_u16_l
#undef tiff_map_scanline_coordinates
#undef tiff_memory_chunk
#undef tiff_memory_chunk_t
#undef tiff_memory_close
#undef tiff_memory_map
#undef tiff_memory_read
#undef tiff_memory_seek
#undef tiff_memory_size
#undef tiff_memory_unmap
#undef tiff_memory_write
#undef tiff_normalize_lab_ab_sample
#undef tiff_normalize_lab_l_sample
#undef tiff_orientation_is_supported
#undef tiff_photometric_allows_embedded_icc
#undef tiff_try_load_high_precision
#undef tiff_logluv_xyz_to_linear
#undef tiff_convert_samples_to_linear
#undef tiff_try_load_integer_alpha
#undef malloc
#undef free
#undef sixel_cms_open_profile_from_mem
#undef sixel_cms_close_profile
#undef TIFFReadRGBAImageOriented
#undef sixel_frame_strip_alpha
#undef sixel_cms_create_srgb_profile
#undef sixel_cms_create_transform
#undef sixel_cms_delete_transform
#undef sixel_cms_do_transform
#undef sixel_cms_convert_to_srgb_with_profile_bytes


static int
lc_reference(void)
{
    sixel_cms_profile_t *source;
    sixel_cms_profile_t *destination;
    sixel_cms_transform_t *transform;
    unsigned char input[64];
    unsigned char output[64];
    size_t i;
    size_t channel;
    int gray;
    int ok;

    source = NULL;
    destination = NULL;
    transform = NULL;
    gray = tl_cases[lc.fixture].photo == 1;
    ok = 0;
    lc_begin(1);
    memcpy(lc.expected, lc.raw, lc.n * 4u);
#if HAVE_LCMS2
    source = sixel_cms_open_profile_from_mem(lc.profile, lc.profile_size);
    destination = sixel_cms_create_srgb_profile();
    if (source == NULL || destination == NULL) { goto cleanup; }
    transform = sixel_cms_create_transform(source,
        gray ? SIXEL_CMS_PIXELFORMAT_GRAY_8 : SIXEL_CMS_PIXELFORMAT_RGBA_8,
        destination,
        gray ? SIXEL_CMS_PIXELFORMAT_RGB_8 : SIXEL_CMS_PIXELFORMAT_RGBA_8,
        gray ? SIXEL_CMS_TRANSFORM_DEFAULT : SIXEL_CMS_TRANSFORM_COPY_ALPHA);
    if (transform == NULL ||
        (int)sixel_cms_transform_engine(transform) != lc.engine) {
        goto cleanup;
    }
    memcpy(input, lc.raw, lc.n * 4u);
    if (gray) {
        for (i = 0u; i < lc.n; ++i) { input[i] = lc.raw[i * 4u]; }
    }
    ok = sixel_cms_do_transform(transform, input, output, lc.n);
    if (!ok) { goto cleanup; }
    for (i = 0u; i < lc.n; ++i) {
        for (channel = 0u; channel < 3u; ++channel) {
            lc.expected[i * 4u + channel] =
                output[i * (gray ? 3u : 4u) + channel];
        }
    }
cleanup:
    if (transform != NULL) { sixel_cms_delete_transform(transform); }
    if (destination != NULL) { sixel_cms_close_profile(destination); }
    if (source != NULL) { sixel_cms_close_profile(source); }
#else
    (void)source; (void)destination; (void)transform; (void)gray;
    (void)output;
    for (i = 0u; i < lc.n; ++i) {
        for (channel = 0u; channel < 3u; ++channel) {
            input[i * 3u + channel] = lc.raw[i * 4u + channel];
        }
    }
    ok = sixel_cms_convert_to_srgb_with_profile_bytes(input,
        tl_cases[lc.fixture].width, tl_cases[lc.fixture].height,
        SIXEL_PIXELFORMAT_RGB888, lc.profile, lc.profile_size);
    for (i = 0u; i < lc.n; ++i) {
        for (channel = 0u; channel < 3u; ++channel) {
            lc.expected[i * 4u + channel] = input[i * 3u + channel];
        }
    }
#endif
    lc_end(ok);
    return ok;
}

/* Integer source families now bypass the legacy public path. Keep their
 * real RGBA decoder and actual CMS-helper fault coverage at helper scope;
 * layout and decoded-YCbCr tests own their public-frame behavior. No
 * obsolete public byte route is forced to preserve these helper checks. */
static SIXELSTATUS
lc_helper_load(sixel_chunk_t *chunk, int cms)
{
    lc_local_25 memory;
    TIFF *tif;
    uint32_t raster[16];
    unsigned char pixels[64];
    size_t i;
    size_t channel;
    int ok;

    memory.buffer = sixel_chunk_get_buffer(chunk);
    memory.size = (toff_t)sixel_chunk_get_size(chunk);
    memory.offset = 0;
    tif = TIFFClientOpen("legacy-helper", "r", (thandle_t)&memory,
        lc_local_28, lc_local_32, lc_local_29, lc_local_26,
        lc_local_30, lc_local_27, lc_local_31);
    if (tif == NULL) { return SIXEL_TIFF_ERROR; }
    ok = lc_rgba(tif, (uint32_t)tl_cases[lc.fixture].width,
        (uint32_t)tl_cases[lc.fixture].height, raster, ORIENTATION_TOPLEFT, 0);
    if (ok) {
        memcpy(pixels, lc.raw, lc.n * 4u);
        if (cms) {
#if HAVE_LCMS2
            lc_local_17(pixels, tl_cases[lc.fixture].width,
                tl_cases[lc.fixture].height, lc.profile,
                (uint32_t)lc.profile_size,
                (uint16_t)tl_cases[lc.fixture].photo);
#else
            lc_local_19(pixels, tl_cases[lc.fixture].width,
                tl_cases[lc.fixture].height, lc.profile,
                (uint32_t)lc.profile_size,
                (uint16_t)tl_cases[lc.fixture].photo);
#endif
        }
        ++lc.calls;
        ++lc.endpoints;
        for (i = 0u; i < lc.n; ++i) {
            for (channel = 0u; channel < 4u; ++channel) {
                lc.ok &= pixels[i * 4u + channel] ==
                    (lc.managed && channel < 3u ? lc.expected : lc.raw)
                    [i * 4u + channel];
            }
        }
    }
    TIFFClose(tif);
    return ok && lc.ok ? SIXEL_OK : SIXEL_TIFF_ERROR;
}

static int
lc_load(sixel_loader_component_t *component, sixel_chunk_t *chunk,
        int cms, int mode, int baseline)
{
    SIXELSTATUS status;
    int applies;
    int managed;
    int ok;

    managed = cms && !tl_cases[lc.fixture].mismatch &&
        (mode == 0 || mode == 6);
    applies = cms && !tl_cases[lc.fixture].mismatch &&
        (mode == 0 || mode == 1 || mode == 6);
    lc_begin(applies);
    lc.mode = mode;
    lc.managed = managed;
    lc.baseline = baseline;
    lc.calls = lc.reads = lc.strips = lc.endpoints = lc.api_calls = 0;
    lc.creates = lc.attempts = lc.injected = lc.real_pixels = 0;
    lc.changed = lc.poisoned = 0;
    status = component->vtbl->setopt(component,
        SIXEL_LOADER_COMPONENT_OPTION_LIBTIFF_ENABLE_CMS, &cms);
    lc.armed = 1;
    if (SIXEL_SUCCEEDED(status)) {
        status = lc_helper_load(chunk, cms);
    }
    lc.armed = 0;
    ok = status == SIXEL_OK && lc.ok && lc.calls == 1 && lc.reads == 1 &&
        lc.strips == (tl_cases[lc.fixture].planar == 2
            ? tl_cases[lc.fixture].channels : 1) && lc.endpoints == 1 &&
        lc.api_calls == applies && lc.allocations == 0 && lc.profiles == 0 &&
        lc.transforms == 0;
    ok &= lc.real_pixels == (applies ? (mode == 1 ? 2 : (int)lc.n) : 0);
    if (mode == 1) { ok &= lc.injected == 1 && lc.changed; }
    if (mode >= 3 && mode <= 5) { ok &= lc.injected == 1; }
    if (mode == 6) { ok &= lc.poisoned == (int)lc.n; }
    fprintf(stderr, "LC1|scope=%s|mode=%d|calls=%d|real=%d|alloc=%d|"
        "profiles=%d|transforms=%d|alpha=%d|ok=%d\n",
        "helper", mode,
        lc.api_calls, lc.real_pixels, lc.allocations, lc.profiles,
        lc.transforms, lc.poisoned, ok);
    lc_end(ok);
    return ok;
}

static int
lc_run(char const *directory, int fixture, int mode)
{
    sixel_allocator_t *allocator;
    sixel_chunk_t *chunk;
    sixel_loader_component_t *component;
    void *object;
    unsigned char original[4096];
    size_t bytes;
    char path[4096];
    int ok;

    allocator = NULL;
    chunk = NULL;
    component = NULL;
    object = NULL;
    ok = 0;
    lc.fixture = fixture;
    lc.n = (size_t)tl_cases[fixture].width * tl_cases[fixture].height;
    lc.ok = 1;
    bytes = 0u;
    snprintf(path, sizeof(path), "%s/tiff-legacy-%s.tif", directory,
             tl_cases[fixture].name);
    if (SIXEL_FAILED(sixel_allocator_new(&allocator,
                                       NULL, NULL, NULL, NULL))) {
        goto cleanup;
    }
    if (SIXEL_FAILED(sixel_chunk_create_from_source(&chunk,
        path, 0, NULL, allocator))) { goto cleanup; }
    bytes = sixel_chunk_get_size(chunk);
    if (bytes > sizeof(original)) { goto cleanup; }
    memcpy(original, sixel_chunk_get_buffer(chunk), bytes);
    if (SIXEL_FAILED(lc_local_8(allocator, &object))) { goto cleanup; }
    component = object;
    if (!lc_load(component, chunk, 0, 0, 1)) { goto cleanup; }
    if (mode == 2) {
        /* A wrong-domain profile must never reach any CMM, even when a
         * neutral witness would coincidentally produce identical bytes. */
        if (!lc_load(component, chunk, 1, 0, 0)) { goto cleanup; }
        if (memcmp(original, sixel_chunk_get_buffer(chunk), bytes) != 0) {
            goto cleanup;
        }
        chunk->vtbl->unref(chunk);
        chunk = NULL;
        fixture -= 5;
        lc.fixture = fixture;
        snprintf(path, sizeof(path), "%s/tiff-legacy-%s.tif", directory,
                 tl_cases[fixture].name);
        if (SIXEL_FAILED(sixel_chunk_create_from_source(&chunk,
            path, 0, NULL, allocator))) { goto cleanup; }
        bytes = sixel_chunk_get_size(chunk);
        if (bytes > sizeof(original)) { goto cleanup; }
        memcpy(original, sixel_chunk_get_buffer(chunk), bytes);
        if (!lc_load(component, chunk, 0, 0, 1)) { goto cleanup; }
    }
    if (!lc_reference() || !lc_load(component, chunk, 1, 0, 0)) {
        goto cleanup;
    }
    if (mode != 0 && mode != 2) {
        if (!lc_load(component, chunk, 1, mode, 0) ||
            !lc_load(component, chunk, 1, 0, 0)) { goto cleanup; }
    }
    ok = memcmp(original, sixel_chunk_get_buffer(chunk), bytes) == 0;
cleanup:
    if (component != NULL) { component->vtbl->unref(component); }
    if (chunk != NULL) { chunk->vtbl->unref(chunk); }
    sixel_allocator_unref(allocator);
    return ok;
}
#endif

int
tiff_legacy_test(int argc, char **argv, int engine, int group, int mode)
{
#if HAVE_LIBTIFF
    sixel_cms_engine_t previous;
    int fixture;
    int selected;
    int ran;
    int wanted;
    int ok;

    if (argc != 2) { return 2; }
#if !HAVE_LCMS2
    if (mode == 4 || mode == 5 || mode == 6) { return 77; }
#endif
    memset(&lc, 0, sizeof(lc));
    lc.engine = engine;
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine((sixel_cms_engine_t)engine);
    if ((int)sixel_cms_get_engine() != engine) {
        sixel_cms_set_engine(previous);
        return 77;
    }
    ran = 0;
    ok = 1;
    wanted = mode == 0 ? 5 : mode == 1 ? (group == 1 ? 4 : 1)
        : mode == 2 ? (group == 1 ? 3 : 1) : 1;
    for (fixture = 0; ok && fixture < 5; ++fixture) {
        selected = mode == 0 || (group == 2 ? fixture == 1 : fixture != 1);
        if (mode == 2) { selected &= fixture < 4; }
        if (mode >= 3) {
            selected &= fixture == (mode == 6 ? 4 : group == 2 ? 1 : 0);
        }
        if (selected) {
            ++ran;
            ok = lc_run(argv[1], fixture + (mode == 2 ? 5 : 0), mode);
        }
    }
    sixel_cms_set_engine(previous);
    printf("LC1|complete|phases=%d\n", lc.phases);
    return ok && ran == wanted ? 0 : 1;
#else
    (void)argc; (void)argv; (void)engine; (void)group; (void)mode;
    return 77;
#endif
}
