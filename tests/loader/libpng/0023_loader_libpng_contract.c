/* Exact PNG/APNG contract probes, independent of compressed SIXEL output. */

#include <math.h>
#include <stdint.h>
#include "tests/loader/pixelformat_test_common.h"
#include "src/loader-common.h"
#include "src/cms.h"
#include "src/loader-manager.h"
#include "tests/loader/builtin/loader_builtin_memory_test_common.h"
#include "tests/test_runner_io.h"

#if HAVE_LIBPNG
/* Fault injection distinguishes optional cache/canvas work from decoding. */
static int lp_allocation_mode;
static int lp_matching_allocations;

static void *
lp_malloc(size_t size)
{
    if ((lp_allocation_mode == 1 && size == 2u * sizeof(void *)) ||
        (lp_allocation_mode == 2 && size == 8u)) {
        ++lp_matching_allocations;
        if (lp_allocation_mode == 1 || lp_matching_allocations == 2) {
            lp_allocation_mode = 0;
            return NULL;
        }
    }
    return malloc(size);
}

static void *
lp_calloc(size_t count, size_t size)
{
    void *memory;

    if (size != 0u && count > SIZE_MAX / size) {
        return NULL;
    }
    memory = lp_malloc(count * size);
    if (memory != NULL) {
        memset(memory, 0, count * size);
    }
    return memory;
}

/* Tiny specimens use stored DEFLATE blocks and standard PNG checksums. */
typedef struct lp_fixture {
    unsigned char bytes[4096];
    size_t size;
} lp_fixture_t;

typedef struct lp_probe {
    int count;
    SIXELSTATUS callback_status;
    int frame_no[8];
    int loop_no[8];
    int delay[8];
    int format[8];
    int hidden[8][2];
    double linear[8][6];
} lp_probe_t;

static void
lp_u32(unsigned char *p, uint32_t value)
{
    p[0] = (unsigned char)(value >> 24);
    p[1] = (unsigned char)(value >> 16);
    p[2] = (unsigned char)(value >> 8);
    p[3] = (unsigned char)value;
}

static void
lp_chunk(lp_fixture_t *f, char const *type,
         unsigned char const *data, size_t size)
{
    uint32_t crc;
    unsigned char *p;
    size_t i;
    int bit;

    p = f->bytes + f->size;
    lp_u32(p, (uint32_t)size);
    memcpy(p + 4, type, 4);
    if (size != 0u) {
        memcpy(p + 8, data, size);
    }
    crc = UINT32_C(0xffffffff);
    for (i = 4u; i < size + 8u; ++i) {
        crc ^= p[i];
        for (bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ ((crc & 1u) ? UINT32_C(0xedb88320) : 0u);
        }
    }
    lp_u32(p + size + 8u, ~crc);
    f->size += size + 12u;
}

static void
lp_header(lp_fixture_t *f, int depth, int type)
{
    unsigned char ihdr[13];

    memcpy(f->bytes, "\211PNG\r\n\032\n", 8);
    f->size = 8u;
    memset(ihdr, 0, sizeof(ihdr));
    lp_u32(ihdr, 2);
    lp_u32(ihdr + 4, 1);
    ihdr[8] = (unsigned char)depth;
    ihdr[9] = (unsigned char)type;
    lp_chunk(f, "IHDR", ihdr, sizeof(ihdr));
}

static void
lp_data(lp_fixture_t *f, int sequence,
        unsigned char const *raw, size_t size)
{
    unsigned char data[256];
    unsigned char *z;
    uint32_t a;
    uint32_t b;
    size_t i;
    size_t prefix;

    prefix = sequence < 0 ? 0u : 4u;
    lp_u32(data, (uint32_t)sequence);
    z = data + prefix;
    z[0] = 0x78;
    z[1] = 0x01;
    z[2] = 0x01;
    z[3] = (unsigned char)size;
    z[4] = 0;
    z[5] = (unsigned char)~size;
    z[6] = 0xff;
    memcpy(z + 7, raw, size);
    a = 1u;
    b = 0u;
    for (i = 0u; i < size; ++i) {
        a = (a + raw[i]) % 65521u;
        b = (b + a) % 65521u;
    }
    lp_u32(z + 7 + size, (b << 16) | a);
    lp_chunk(f, sequence < 0 ? "IDAT" : "fdAT", data,
             prefix + size + 11u);
}

static void
lp_actl(lp_fixture_t *f, int frames, int plays)
{
    unsigned char data[8];

    lp_u32(data, (uint32_t)frames);
    lp_u32(data + 4, (uint32_t)plays);
    lp_chunk(f, "acTL", data, sizeof(data));
}

static void
lp_fctl(lp_fixture_t *f, int sequence, int blend, int dispose)
{
    unsigned char data[26];

    memset(data, 0, sizeof(data));
    lp_u32(data, (uint32_t)sequence);
    lp_u32(data + 4, 2);
    lp_u32(data + 8, 1);
    data[21] = 1;
    data[23] = 10;
    data[24] = (unsigned char)dispose;
    data[25] = (unsigned char)blend;
    lp_chunk(f, "fcTL", data, sizeof(data));
}

static double
lp_linear(double value)
{
    return value <= 0.04045 ? value / 12.92
         : pow((value + 0.055) / 1.055, 2.4);
}

static SIXELSTATUS
lp_capture(sixel_frame_t *frame, void *data)
{
    lp_probe_t *probe;
    sixel_frame_pixels_view_t view;
    sixel_frame_transparency_t tr;
    unsigned char const *bytes;
    float const *floats;
    int n;
    int x;
    int c;
    int index;
    double value;
    SIXELSTATUS status;

    probe = (lp_probe_t *)data;
    n = probe->count++;
    if (n >= 8 || frame->width != 2 || frame->height != 1) {
        return SIXEL_BAD_INPUT;
    }
    status = loader_test_frame_get_pixels_view(frame, &view);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    status = sixel_frame_as_interface(frame)->vtbl->get_transparency(
        sixel_frame_as_interface(frame), &tr);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    probe->frame_no[n] = sixel_frame_get_frame_no(frame);
    probe->loop_no[n] = sixel_frame_get_loop_no(frame);
    probe->delay[n] = sixel_frame_get_delay(frame);
    probe->format[n] = frame->pixelformat;
    bytes = view.pixels;
    floats = view.pixels_float32;
    for (x = 0; x < 2; ++x) {
        probe->hidden[n][x] = tr.transparent_mask != NULL
            && tr.transparent_mask_size > (size_t)x
            && tr.transparent_mask[x] != 0;
        if (frame->pixelformat == SIXEL_PIXELFORMAT_RGBA8888) {
            probe->hidden[n][x] |= tr.alpha_zero_is_transparent
                && bytes[x * 4 + 3] == 0;
        }
        for (c = 0; c < 3; ++c) {
            if (SIXEL_PIXELFORMAT_IS_FLOAT32(frame->pixelformat)) {
                value = floats[x * 3 + c];
                if (frame->pixelformat != SIXEL_PIXELFORMAT_LINEARRGBFLOAT32) {
                    value = lp_linear(value);
                }
            } else if (frame->pixelformat == SIXEL_PIXELFORMAT_PAL8) {
                index = bytes[x];
                value = lp_linear(frame->palette[index * 3 + c] / 255.0);
                probe->hidden[n][x] |= index == tr.transparent;
            } else if (frame->pixelformat == SIXEL_PIXELFORMAT_RGB888 ||
                       frame->pixelformat == SIXEL_PIXELFORMAT_RGBA8888) {
                index = x * (frame->pixelformat ==
                             SIXEL_PIXELFORMAT_RGBA8888 ? 4 : 3) + c;
                value = lp_linear(bytes[index] / 255.0);
            } else {
                return SIXEL_BAD_INPUT;
            }
            probe->linear[n][x * 3 + c] = value;
        }
    }
    return probe->callback_status;
}

static SIXELSTATUS
lp_load(lp_fixture_t const *f, lp_probe_t *probe,
        int cms, int colors, unsigned char const *bg, int loops,
        int through_manager)
{
    sixel_allocator_t *allocator;
    sixel_chunk_t *chunk;
    sixel_loader_component_t *component;
    loader_probe_callback_state_t callback;
    SIXELSTATUS status;
    int use_palette;
    sixel_loader_manager_t *manager;
    sixel_loader_manager_build_request_t request;
    sixel_option_value_schema_t defs[2];
    sixel_option_argument_list_item_t items[2];
    sixel_option_argument_list_resolution_t resolution;

    allocator = NULL;
    chunk = NULL;
    component = NULL;
    use_palette = 1;
    manager = NULL;
    memset(&request, 0, sizeof(request));
    memset(defs, 0, sizeof(defs));
    memset(items, 0, sizeof(items));
    memset(&resolution, 0, sizeof(resolution));
    memset(probe, 0, sizeof(*probe));
    callback.loader = NULL;
    callback.fn = lp_capture;
    callback.context = probe;
    status = sixel_allocator_new(&allocator, lp_malloc, lp_calloc,
                                  realloc, free);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_chunk_create_from_memory(&chunk, f->bytes, f->size,
                                            NULL, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (through_manager > 0) {
        probe->callback_status = through_manager == 1
                               ? SIXEL_BAD_INPUT : SIXEL_OK;
        if (through_manager == 3) {
            probe->callback_status = SIXEL_FALSE;
        }
        defs[0].name = "libpng";
        defs[1].name = "builtin";
        items[0].resolution.base_def = &defs[0];
        items[1].resolution.base_def = &defs[1];
        resolution.items = items;
        resolution.item_count = 2u;
        resolution.has_trailing_bang = 1;
        request.resolution = &resolution;
        request.reqcolors = 256;
        request.loop_control = SIXEL_LOOP_DISABLE;
        status = sixel_loader_manager_new(allocator, (void **)&manager);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        status = manager->vtbl->build_chain(manager, &request);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        status = manager->vtbl->load(manager, chunk, NULL,
            capture_frame_trampoline, &callback);
        goto end;
    }
    status = create_loader_component_by_name("libpng", allocator,
                                              (void **)&component);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_loader_component_setopt(component,
        SIXEL_LOADER_COMPONENT_OPTION_CMS_ENGINE, &cms);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    (void)sixel_loader_component_setopt(component,
        SIXEL_LOADER_OPTION_USE_PALETTE, &use_palette);
    (void)sixel_loader_component_setopt(component,
        SIXEL_LOADER_OPTION_REQCOLORS, &colors);
    (void)sixel_loader_component_setopt(component,
        SIXEL_LOADER_OPTION_LOOP_CONTROL, &loops);
    (void)sixel_loader_component_setopt(component,
        SIXEL_LOADER_OPTION_BGCOLOR, bg);
    lp_matching_allocations = 0;
    lp_allocation_mode = through_manager == -1 ? 1 :
                         through_manager == -2 ? 2 : 0;
    status = sixel_loader_component_load(component, chunk,
        capture_frame_trampoline, &callback);
end:
    lp_allocation_mode = 0;
    if (manager != NULL) {
        manager->vtbl->unref(manager);
    }
    sixel_loader_component_unref(component);
    if (chunk != NULL) {
        chunk->vtbl->unref(chunk);
    }
    sixel_allocator_unref(allocator);
    return status;
}
#endif

int
test_loader_libpng_contract(int argc, char **argv)
{
#if HAVE_LIBPNG
    lp_fixture_t f;
    lp_fixture_t reference;
    lp_probe_t reference_probe;
    lp_probe_t probe;
    SIXELSTATUS status;
    unsigned char opaque[7] = {0, 255, 0, 0, 0, 255, 0};
    unsigned char animated[7] = {0, 0, 0, 255, 0, 255, 0};
    unsigned char trns[6] = {0, 255, 0, 0, 0, 0};
    unsigned char rgba[9] = {0, 255, 0, 0, 0, 0, 255, 0, 255};
    unsigned char rgba16[17] = {
        0, 0x80, 1, 0, 0, 0, 0, 255, 255,
        0x80, 2, 0, 0, 0, 0, 255, 255
    };
    unsigned char half[9] = {0, 0, 0, 255, 128, 0, 0, 255, 128};
    unsigned char palette[6] = {255, 0, 0, 0, 255, 0};
    unsigned char indexes[3] = {0, 0, 1};
    unsigned char alphas[2] = {0, 255};
    unsigned char background[6] = {0, 0, 0, 0, 0, 255};
    unsigned char external[3] = {255, 0, 0};
    unsigned char gamma[4] = {0, 1, 0x86, 0xa0};
    unsigned char mid[7] = {0, 128, 128, 128, 128, 128, 128};
    unsigned char const *bg;
    int cms;
    int colors;
    unsigned char metadata[EDGE_BUFFER_CAPACITY];
    unsigned char gray16[5] = {0, 0x80, 1, 0x80, 2};
    unsigned char grayalpha16[9] = {0, 0x80, 1, 255, 255,
                                    0x80, 2, 255, 255};
    unsigned char invalid_zlib[7] = {0, 0, 0, 2, 0, 0, 0};
    size_t metadata_size;
    size_t offset;
    size_t length;
    size_t idat_start;
    int mode;
    int loops;
    int j;
    int result;
    FILE *output;

    mode = 0;
    loops = SIXEL_LOOP_DISABLE;
    bg = NULL;
    cms = SIXEL_CMS_ENGINE_NONE;
    colors = 256;
    if (argc != 2 &&
        (argc != 3 || strcmp(argv[1], "emit_trns") != 0)) {
        return 1;
    }
    lp_header(&f, 8, 2);
    if (strcmp(argv[1], "rejected_icc") == 0) {
        lp_header(&f, 8, 6);
        lp_header(&reference, 8, 6);
        if (edge_read_fixture("/tests/data/colormgmt/input/custom/"
            "rgba_mab_valid.png", metadata, sizeof(metadata),
            &metadata_size) != 0) {
            return 1;
        }
        for (offset = 8u; offset + 12u <= metadata_size;
             offset += length + 12u) {
            length = ((size_t)metadata[offset] << 24) |
                     ((size_t)metadata[offset + 1u] << 16) |
                     ((size_t)metadata[offset + 2u] << 8) |
                     metadata[offset + 3u];
            if (length > metadata_size - offset - 12u) {
                return 1;
            }
            if (memcmp(metadata + offset + 4u, "iCCP", 4u) == 0) {
                lp_chunk(&f, "iCCP", metadata + offset + 8u, length);
            } else if (memcmp(metadata + offset + 4u, "cHRM", 4u) == 0) {
                lp_chunk(&f, "cHRM", metadata + offset + 8u, length);
                lp_chunk(&reference, "cHRM", metadata + offset + 8u, length);
            }
        }
        lp_data(&f, -1, rgba, sizeof(rgba));
        lp_data(&reference, -1, rgba, sizeof(rgba));
        lp_chunk(&reference, "IEND", NULL, 0u);
        cms = SIXEL_CMS_ENGINE_BUILTIN;
    } else if (strcmp(argv[1], "gray_icc") == 0 ||
        strcmp(argv[1], "apng_gray_icc") == 0) {
        lp_header(&f, 16, strcmp(argv[1], "gray_icc") == 0 ? 0 : 4);
        if (edge_read_fixture("/tests/data/colormgmt/input/png/gray/"
            "img_gray_icc1_srgb0_chrm0_gama0.png", metadata,
            sizeof(metadata), &metadata_size) != 0) {
            return 1;
        }
        for (offset = 8u; offset + 12u <= metadata_size;
             offset += length + 12u) {
            length = ((size_t)metadata[offset] << 24) |
                     ((size_t)metadata[offset + 1u] << 16) |
                     ((size_t)metadata[offset + 2u] << 8) |
                     metadata[offset + 3u];
            if (length > metadata_size - offset - 12u) {
                return 1;
            }
            if (memcmp(metadata + offset + 4u, "iCCP", 4u) == 0) {
                lp_chunk(&f, "iCCP", metadata + offset + 8u, length);
                break;
            }
        }
        if (strcmp(argv[1], "apng_gray_icc") == 0) {
            lp_actl(&f, 1, 1);
            lp_fctl(&f, 0, 0, 0);
            lp_data(&f, -1, grayalpha16, sizeof(grayalpha16));
        } else {
            lp_data(&f, -1, gray16, sizeof(gray16));
        }
        cms = SIXEL_CMS_ENGINE_AUTO;
    } else if (strcmp(argv[1], "cache") == 0 ||
               strcmp(argv[1], "cache_failure") == 0 ||
               strcmp(argv[1], "dispose") == 0 ||
               strcmp(argv[1], "previous") == 0 ||
               strcmp(argv[1], "late_error") == 0) {
        lp_header(&f, 8, 6);
        lp_actl(&f, 2, 3);
        lp_fctl(&f, 0, 0, strcmp(argv[1], "dispose") == 0 ? 1 :
                          strcmp(argv[1], "previous") == 0 ? 2 : 0);
        lp_data(&f, -1, rgba, sizeof(rgba));
        lp_fctl(&f, 1, 1, 0);
        half[4] = 0;
        if (strcmp(argv[1], "late_error") == 0) {
            lp_chunk(&f, "fdAT", invalid_zlib, sizeof(invalid_zlib));
            mode = 2;
        } else {
            lp_data(&f, 2, half, sizeof(half));
        }
        if (strncmp(argv[1], "cache", 5) == 0) {
            mode = strcmp(argv[1], "cache_failure") == 0 ? -1 : 0;
            loops = SIXEL_LOOP_AUTO;
        }
    } else if (strcmp(argv[1], "indexed_partial") == 0) {
        lp_header(&f, 8, 3);
        alphas[0] = 128;
        lp_chunk(&f, "PLTE", palette, sizeof(palette));
        lp_chunk(&f, "tRNS", alphas, sizeof(alphas));
        lp_data(&f, -1, indexes, sizeof(indexes));
        external[0] = 0;
        external[2] = 255;
        bg = external;
    } else if (strcmp(argv[1], "keep_background") == 0 ||
               strcmp(argv[1], "composite_missing") == 0) {
        lp_header(&f, 8, 6);
        lp_data(&f, -1, rgba, sizeof(rgba));
        cms = SIXEL_CMS_ENGINE_BUILTIN;
        if (strcmp(argv[1], "keep_background") == 0) {
            bg = external;
            sixel_helper_set_loader_transparent_policy(SIXEL_ALPHA_POLICY_KEEP);
        } else {
            sixel_helper_set_loader_transparent_policy(
                SIXEL_ALPHA_POLICY_COMPOSITE);
        }
    } else if (strcmp(argv[1], "static_alloc") == 0 ||
               strcmp(argv[1], "literal_actl") == 0 ||
               strcmp(argv[1], "split_idat") == 0 ||
               strcmp(argv[1], "orphan") == 0 ||
               strcmp(argv[1], "ancillary_crc") == 0 ||
               strcmp(argv[1], "truncated") == 0) {
        if (strcmp(argv[1], "orphan") == 0) {
            lp_fctl(&f, 0, 0, 0);
        }
        if (strcmp(argv[1], "literal_actl") == 0 ||
            strcmp(argv[1], "ancillary_crc") == 0) {
            lp_chunk(&f, "tEXt", (unsigned char const *)"Comment\0acTL", 12u);
            if (strcmp(argv[1], "ancillary_crc") == 0) {
                f.bytes[f.size - 1u] ^= 1u;
            }
        }
        idat_start = f.size;
        lp_data(&f, -1, opaque, sizeof(opaque));
        if (strcmp(argv[1], "split_idat") == 0) {
            memcpy(metadata, f.bytes + idat_start + 8u, 18u);
            f.size = idat_start;
            lp_chunk(&f, "IDAT", metadata, 5u);
            lp_chunk(&f, "IDAT", metadata + 5u, 13u);
        }
        mode = strcmp(argv[1], "static_alloc") == 0 ? -2 : 0;
    } else if (strcmp(argv[1], "cms_mask") == 0) {
        lp_header(&f, 8, 6);
        lp_data(&f, -1, rgba, sizeof(rgba));
        cms = SIXEL_CMS_ENGINE_BUILTIN;
    } else if (strcmp(argv[1], "indexed_mask") == 0) {
        lp_header(&f, 8, 3);
        lp_chunk(&f, "PLTE", palette, sizeof(palette));
        lp_chunk(&f, "tRNS", alphas, sizeof(alphas));
        lp_data(&f, -1, indexes, sizeof(indexes));
        colors = 1;
    } else if (strcmp(argv[1], "background") == 0 ||
               strcmp(argv[1], "apng_background") == 0) {
        lp_header(&f, 8, 6);
        lp_chunk(&f, "bKGD", background, sizeof(background));
        if (strcmp(argv[1], "apng_background") == 0) {
            lp_actl(&f, 1, 1);
            lp_fctl(&f, 0, 0, 0);
        }
        lp_data(&f, -1, rgba, sizeof(rgba));
        bg = external;
        sixel_helper_set_loader_transparent_policy(
            SIXEL_ALPHA_POLICY_COMPOSITE);
    } else if (strcmp(argv[1], "lowbits") == 0 ||
               strcmp(argv[1], "apng_lowbits") == 0) {
        lp_header(&f, 16, 6);
        if (strcmp(argv[1], "apng_lowbits") == 0) {
            lp_actl(&f, 1, 1);
            lp_fctl(&f, 0, 0, 0);
        }
        lp_data(&f, -1, rgba16, sizeof(rgba16));
    } else if (strcmp(argv[1], "apng_gamma") == 0) {
        lp_chunk(&f, "gAMA", gamma, sizeof(gamma));
        lp_actl(&f, 1, 1);
        lp_fctl(&f, 0, 0, 0);
        lp_data(&f, -1, mid, sizeof(mid));
        cms = SIXEL_CMS_ENGINE_BUILTIN;
    } else if (strcmp(argv[1], "over") == 0) {
        lp_header(&f, 8, 6);
        lp_actl(&f, 2, 1);
        lp_fctl(&f, 0, 0, 0);
        rgba[4] = 255;
        lp_data(&f, -1, rgba, sizeof(rgba));
        lp_fctl(&f, 1, 1, 0);
        lp_data(&f, 2, half, sizeof(half));
    } else if (strcmp(argv[1], "default") == 0) {
        lp_actl(&f, 1, 1);
        idat_start = f.size;
        lp_data(&f, -1, opaque, sizeof(opaque));
        /* Exclusion applies to the complete default stream, not one IDAT. */
        memcpy(metadata, f.bytes + idat_start + 8u, 18u);
        f.size = idat_start;
        lp_chunk(&f, "IDAT", metadata, 5u);
        lp_chunk(&f, "IDAT", metadata + 5u, 13u);
        lp_fctl(&f, 0, 0, 0);
        lp_data(&f, 1, animated, sizeof(animated));
    } else if (strcmp(argv[1], "trns") == 0 ||
               strcmp(argv[1], "emit_trns") == 0) {
        lp_chunk(&f, "tRNS", trns, sizeof(trns));
        lp_actl(&f, 1, 1);
        lp_fctl(&f, 0, 0, 0);
        lp_data(&f, -1, opaque, sizeof(opaque));
    } else if (strcmp(argv[1], "crc") == 0) {
        lp_actl(&f, 1, 1);
        lp_fctl(&f, 0, 0, 0);
        lp_data(&f, -1, opaque, sizeof(opaque));
        f.bytes[f.size - 1u] ^= 1u;
    } else if (strcmp(argv[1], "iend") == 0 ||
               strcmp(argv[1], "fallback") == 0) {
        lp_data(&f, -1, opaque, sizeof(opaque));
    } else {
        return 1;
    }
    lp_chunk(&f, "IEND", NULL, 0u);
    if (strcmp(argv[1], "emit_trns") == 0) {
        /* Emscripten's Node stdout path is text-oriented.  Let callers write
         * binary PNG bytes through NODERAWFS instead of a shell pipeline. */
        output = stdout;
        if (argc == 3) {
            output = test_runner_fopen(argv[2], "wb");
            if (output == NULL) {
                return 1;
            }
        }
        result = fwrite(f.bytes, 1u, f.size, output) != f.size;
        if (output != stdout && fclose(output) != 0) {
            result = 1;
        }
        return result;
    }
    if (strcmp(argv[1], "iend") == 0) {
        f.bytes[f.size - 1u] ^= 1u;
    }
    if (strcmp(argv[1], "truncated") == 0) {
        f.size -= 12u;
    }
    if (strcmp(argv[1], "fallback") == 0) {
        mode = 1;
    }
    status = lp_load(&f, &probe, cms, colors, bg, loops, mode);
    result = 1;
    if (strcmp(argv[1], "rejected_icc") == 0) {
        result = status != SIXEL_OK || probe.count != 1;
        status = lp_load(&reference, &reference_probe, cms, colors, NULL,
                         SIXEL_LOOP_DISABLE, 0);
        result |= status != SIXEL_OK || reference_probe.count != 1 ||
            memcmp(probe.linear, reference_probe.linear,
                   sizeof(probe.linear)) != 0 || !probe.hidden[0][0];
    } else if (strcmp(argv[1], "gray_icc") == 0 ||
        strcmp(argv[1], "apng_gray_icc") == 0) {
        /* ICC's rounded D50 and the sRGB adaptation matrix differ slightly.
         * Bound that white-point error separately from the low-bit contrast. */
        result = status != SIXEL_OK || probe.count != 1 ||
            fabs(probe.linear[0][0] - 32769.0 / 65535.0) > 0.0005 ||
            fabs(probe.linear[0][1] - 32769.0 / 65535.0) > 0.0005 ||
            fabs(probe.linear[0][2] - 32769.0 / 65535.0) > 0.0005 ||
            probe.linear[0][3] - probe.linear[0][0] < 0.00001;
    } else if (strncmp(argv[1], "cache", 5) == 0) {
        result = status != SIXEL_OK || probe.count != 6;
        for (j = 0; j < 6; ++j) {
            result |= probe.frame_no[j] != j % 2 ||
                probe.loop_no[j] != j / 2 || probe.delay[j] != 10 ||
                !probe.hidden[j][0] || probe.hidden[j][1] ||
                fabs(probe.linear[j][4] - (j % 2 ? 127.0 / 255.0 : 1.0))
                    > 1e-6;
        }
        if (mode == -1) {
            result |= lp_matching_allocations != 1;
        }
    } else if (strcmp(argv[1], "dispose") == 0 ||
               strcmp(argv[1], "previous") == 0) {
        result = status != SIXEL_OK || probe.count != 2 ||
            !probe.hidden[1][0] || probe.hidden[1][1] ||
            probe.linear[1][4] != 0.0 || probe.linear[1][5] != 1.0;
    } else if (strcmp(argv[1], "late_error") == 0) {
        result = !SIXEL_FAILED(status) || probe.count != 1;
    } else if (strcmp(argv[1], "indexed_partial") == 0) {
        result = status != SIXEL_OK || probe.count != 1 ||
            fabs(probe.linear[0][0] - 128.0 / 255.0) > 1e-6 ||
            fabs(probe.linear[0][2] - 127.0 / 255.0) > 1e-6;
    } else if (strcmp(argv[1], "keep_background") == 0 ||
               strcmp(argv[1], "composite_missing") == 0) {
        result = status != SIXEL_OK || probe.count != 1 ||
            !probe.hidden[0][0] || probe.hidden[0][1];
    } else if (strcmp(argv[1], "static_alloc") == 0 ||
               strcmp(argv[1], "literal_actl") == 0 ||
               strcmp(argv[1], "split_idat") == 0 ||
               strcmp(argv[1], "ancillary_crc") == 0) {
        result = status != SIXEL_OK || probe.count != 1 ||
            probe.linear[0][0] != 1.0 || probe.linear[0][4] != 1.0;
        if (mode == -2) {
            result |= lp_matching_allocations > 1;
        }
    } else if (strcmp(argv[1], "fallback") == 0) {
        result = status != SIXEL_BAD_INPUT || probe.count != 1;
        /* Generic callback failure must also stay terminal inside APNG. */
        lp_header(&reference, 8, 2);
        lp_actl(&reference, 1, 1);
        lp_fctl(&reference, 0, 0, 0);
        lp_data(&reference, -1, opaque, sizeof(opaque));
        lp_chunk(&reference, "IEND", NULL, 0u);
        status = lp_load(&reference, &probe, cms, colors, NULL, loops, 3);
        result |= status != SIXEL_FALSE || probe.count != 1;
    } else if (strcmp(argv[1], "cms_mask") == 0 ||
        strcmp(argv[1], "indexed_mask") == 0) {
        result = status != SIXEL_OK || probe.count != 1 ||
            !probe.hidden[0][0] || probe.hidden[0][1];
    } else if (strcmp(argv[1], "background") == 0 ||
               strcmp(argv[1], "apng_background") == 0) {
        result = status != SIXEL_OK || probe.count != 1 ||
            probe.hidden[0][0] || probe.linear[0][0] != 0.0 ||
            probe.linear[0][2] != 1.0;
    } else if (strcmp(argv[1], "lowbits") == 0 ||
               strcmp(argv[1], "apng_lowbits") == 0) {
        result = status != SIXEL_OK || probe.count != 1 ||
            !SIXEL_PIXELFORMAT_IS_FLOAT32(probe.format[0]) ||
            fabs(probe.linear[0][0] - lp_linear(32769.0 / 65535.0)) > 1e-7 ||
            fabs(probe.linear[0][3] - lp_linear(32770.0 / 65535.0)) > 1e-7;
    } else if (strcmp(argv[1], "apng_gamma") == 0) {
        result = status != SIXEL_OK || probe.count != 1 ||
            fabs(probe.linear[0][0] - 128.0 / 255.0) > 0.0001;
        /* A misplaced gAMA after default IDAT must not become shared color
         * metadata when a later rectangle is reconstructed as a PNG. */
        lp_header(&reference, 8, 2);
        lp_actl(&reference, 1, 1);
        lp_data(&reference, -1, opaque, sizeof(opaque));
        lp_chunk(&reference, "gAMA", gamma, sizeof(gamma));
        lp_fctl(&reference, 0, 0, 0);
        lp_data(&reference, 1, mid, sizeof(mid));
        lp_chunk(&reference, "IEND", NULL, 0u);
        status = lp_load(&reference, &probe, cms, colors, NULL, loops, 0);
        result |= status != SIXEL_OK || probe.count != 1 ||
            fabs(probe.linear[0][0] - lp_linear(128.0 / 255.0)) > 1e-6;
    } else if (strcmp(argv[1], "over") == 0) {
        result = status != SIXEL_OK || probe.count != 2 ||
            fabs(probe.linear[1][0] - 127.0 / 255.0) > 1e-6 ||
            fabs(probe.linear[1][2] - 128.0 / 255.0) > 1e-6;
    } else if (strcmp(argv[1], "default") == 0) {
        result = status != SIXEL_OK || probe.count != 1 ||
            probe.frame_no[0] != 0 || probe.delay[0] != 10 ||
            probe.linear[0][0] != 0.0 || probe.linear[0][2] != 1.0;
    } else if (strcmp(argv[1], "trns") == 0) {
        result = status != SIXEL_OK || probe.count != 1 ||
            !probe.hidden[0][0] || probe.hidden[0][1];
    } else {
        result = !SIXEL_FAILED(status) || probe.count != 0;
    }
    if (result) {
        fprintf(stderr, "%s: status=%x frames=%d hidden=%d,%d rgb=%g,%g,%g\n",
                argv[1], status, probe.count,
                probe.hidden[0][0], probe.hidden[0][1],
                probe.linear[0][0], probe.linear[0][1], probe.linear[0][2]);
    }
    return result;
#else
    (void)argc;
    (void)argv;
    return SIXEL_TEST_SKIP;
#endif
}
