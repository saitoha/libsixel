/* SPDX-License-Identifier: MIT */
/* Independent original-sample, linear composition and canvas-state probes. */
#include <math.h>
#include <stdint.h>
#include "tests/_support/loader/pixelformat_test_common.h"
#include "src/loader-common.h"
#include "src/loader-manager.h"
#include "src/cms.h"

typedef struct ap_fixture {
    unsigned char bytes[4096];
    size_t size;
} ap_fixture_t;

typedef struct ap_probe {
    int count;
    int format[8];
    int frame[8];
    int loop[8];
    int hidden[8][2];
    double rgb[8][6];
    SIXELSTATUS callback_status;
} ap_probe_t;

static void
ap_u32(unsigned char *p, uint32_t value)
{
    p[0] = (unsigned char)(value >> 24);
    p[1] = (unsigned char)(value >> 16);
    p[2] = (unsigned char)(value >> 8);
    p[3] = (unsigned char)value;
}

static void
ap_chunk(ap_fixture_t *f, char const *type,
         unsigned char const *data, size_t size)
{
    uint32_t crc;
    unsigned char *p;
    size_t i;
    int bit;

    p = f->bytes + f->size;
    ap_u32(p, (uint32_t)size);
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
    ap_u32(p + size + 8u, ~crc);
    f->size += size + 12u;
}

static void
ap_header(ap_fixture_t *f, int depth, int type)
{
    unsigned char ihdr[13];

    memcpy(f->bytes, "\211PNG\r\n\032\n", 8);
    f->size = 8u;
    memset(ihdr, 0, sizeof(ihdr));
    ap_u32(ihdr, 2);
    ap_u32(ihdr + 4, 1);
    ihdr[8] = (unsigned char)depth;
    ihdr[9] = (unsigned char)type;
    ap_chunk(f, "IHDR", ihdr, sizeof(ihdr));
}

static void
ap_data(ap_fixture_t *f, int sequence,
        unsigned char const *raw, size_t size)
{
    unsigned char data[256];
    unsigned char *z;
    uint32_t a;
    uint32_t b;
    size_t i;
    size_t prefix;

    prefix = sequence < 0 ? 0u : 4u;
    ap_u32(data, (uint32_t)sequence);
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
    ap_u32(z + 7 + size, (b << 16) | a);
    ap_chunk(f, sequence < 0 ? "IDAT" : "fdAT", data,
             prefix + size + 11u);
}

static void
ap_actl(ap_fixture_t *f, int frames, int plays)
{
    unsigned char data[8];

    ap_u32(data, (uint32_t)frames);
    ap_u32(data + 4, (uint32_t)plays);
    ap_chunk(f, "acTL", data, sizeof(data));
}

static void
ap_fctl(ap_fixture_t *f, int sequence, int blend, int dispose,
        int width, int x)
{
    unsigned char data[26];

    memset(data, 0, sizeof(data));
    ap_u32(data, (uint32_t)sequence);
    ap_u32(data + 4, (uint32_t)width);
    ap_u32(data + 8, 1);
    ap_u32(data + 12, (uint32_t)x);
    data[21] = 1;
    data[23] = 10;
    data[24] = (unsigned char)dispose;
    data[25] = (unsigned char)blend;
    ap_chunk(f, "fcTL", data, sizeof(data));
}


static double
ap_linear(double v)
{
    return v <= 0.04045 ? v / 12.92 : pow((v + 0.055) / 1.055, 2.4);
}

static SIXELSTATUS
ap_capture(sixel_frame_t *frame, void *data)
{
    ap_probe_t *probe;
    sixel_frame_interface_t *iface;
    sixel_frame_pixels_view_t view;
    sixel_frame_transparency_t mask;
    sixel_frame_alpha_t alpha;
    size_t i;
    int n;

    probe = (ap_probe_t *)data;
    n = probe->count++;
    iface = sixel_frame_as_interface(frame);
    iface->vtbl->get_pixels(iface, &view);
    iface->vtbl->get_transparency(iface, &mask);
    iface->vtbl->get_alpha(iface, &alpha);
    /* Fractional coverage must be resolved before the legacy callback. */
    if (n >= 8 || view.width != 2 || view.height != 1 ||
        alpha.values != NULL || alpha.count != 0u ||
        (view.pixelformat != SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 &&
         view.pixelformat != SIXEL_PIXELFORMAT_RGB888)) {
        return SIXEL_BAD_INPUT;
    }
    probe->format[n] = view.pixelformat;
    probe->frame[n] = sixel_frame_get_frame_no(frame);
    probe->loop[n] = sixel_frame_get_loop_no(frame);
    for (i = 0u; i < 6u; ++i) {
        probe->rgb[n][i] = view.pixelformat == SIXEL_PIXELFORMAT_RGB888
            ? ap_linear(view.pixels[i] / 255.0)
            : view.pixels_float32[i];
    }
    for (i = 0u; i < 2u; ++i) {
        probe->hidden[n][i] = mask.transparent_mask != NULL
            && mask.transparent_mask_size == 2u && mask.transparent_mask[i];
    }
    return probe->callback_status;
}

/* Fixed two-pixel RGBA rows, with no production serializer or CMS oracle. */
static void
ap_row(ap_fixture_t *f, int sequence, int depth, unsigned const *values)
{
    unsigned char row[17];
    size_t i;

    row[0] = 0;
    for (i = 0u; i < 8u; ++i) {
        if (depth == 16) {
            row[1u + i * 2u] = (unsigned char)(values[i] >> 8);
            row[2u + i * 2u] = (unsigned char)values[i];
        } else {
            row[1u + i] = (unsigned char)values[i];
        }
    }
    ap_data(f, sequence, row, depth == 16 ? 17u : 9u);
}

static void
ap_build(ap_fixture_t *f, int mode)
{
    static unsigned const rows[4][8] = {
        {65535, 0, 0, 32768, 0, 65535, 0, 0},
        {0, 65535, 0, 16384, 0, 0, 65535, 0},
        {0, 0, 0, 0, 0, 0, 65535, 65535},
        {0, 65535, 0, 0, 65535, 0, 0, 0}
    };
    static unsigned const precise[8] = {
        1, 257, 32768, 65535, 32769, 65534, 65535, 65535
    };
    static unsigned const tiny[8] = {
        65535, 0, 0, 1, 0, 65535, 0, 257
    };
    static unsigned const byte_first[8] = {
        64, 128, 192, 255, 48, 32, 16, 0
    };
    static unsigned const byte_next[8] = {
        0, 255, 0, 128, 255, 0, 0, 0
    };
    static unsigned char const cicp[4] = {12, 13, 0, 1};
    static unsigned char const background[6] = {255, 255, 0, 0, 0, 0};
    unsigned char gamma[4];
    unsigned char raw[13];
    static unsigned char const palette[6] = {255, 0, 0, 0, 255, 0};
    static unsigned char const trns[6] = {0, 1, 1, 1, 128, 0};
    int depth;
    int frames;
    int n;

    depth = mode == 7 || mode == 8 ? 8 : 16;
    frames = mode <= 1 || mode == 7 || mode == 13 ? 1
           : mode == 8 ? 2 : 4;
    if (mode == 16 || mode == 17) {
        ap_header(f, 8, 6);
        ap_actl(f, mode == 16 ? 2 : 3, 1);
        ap_fctl(f, 0, 0, 0, 2, 0);
        ap_row(f, -1, 8, (unsigned const[8]){
            255, 0, 0, 255, 255, 0, 0, 255});
        ap_fctl(f, 1, mode == 16, mode == 17, 1, 1);
        ap_data(f, 2, (unsigned char const[5]){0, 0, 0, 255, 128}, 5u);
        if (mode == 17) {
            ap_fctl(f, 3, 1, 0, 1, 1);
            ap_data(f, 4, (unsigned char const[5]){0, 0, 255, 0, 0}, 5u);
        }
        ap_chunk(f, "IEND", NULL, 0u);
        return;
    }
    if (mode == 14 || mode == 15) {
        ap_header(f, mode == 14 ? 8 : 16, mode == 14 ? 3 : 2);
        if (mode == 14) {
            ap_chunk(f, "PLTE", palette, sizeof(palette));
            ap_chunk(f, "tRNS", trns + 4, 2u);
        } else {
            ap_chunk(f, "tRNS", trns, sizeof(trns));
        }
        ap_actl(f, 1, 1);
        ap_fctl(f, 0, 0, 0, 2, 0);
        memset(raw, 0, sizeof(raw));
        if (mode == 14) {
            raw[2] = 1;
        } else {
            memcpy(raw + 1, trns, 6u);
            memcpy(raw + 7, trns, 6u);
            raw[12] = 1;
        }
        ap_data(f, -1, raw, mode == 14 ? 3u : sizeof(raw));
        ap_chunk(f, "IEND", NULL, 0u);
        return;
    }
    ap_header(f, depth, 6);
    if (depth == 16 && mode >= 2 && mode != 13) {
        ap_chunk(f, "cICP", cicp, sizeof(cicp));
    }
    if (mode == 12) {
        ap_chunk(f, "bKGD", background, sizeof(background));
    }
    if (mode == 13) {
        ap_u32(gamma, 100000u);
        ap_chunk(f, "gAMA", gamma, sizeof(gamma));
    }
    ap_actl(f, frames, mode == 6 ? 2 : 1);
    for (n = 0; n < frames; ++n) {
        ap_fctl(f, n == 0 ? 0 : n * 2 - 1, n != 0,
                n == 1 && depth == 16 ? 2 : n == 2 ? 1 : 0, 2, 0);
        ap_row(f, n == 0 ? -1 : n * 2, depth,
            mode == 0 || mode == 13 ? precise : mode == 1 ? tiny
            : depth == 8 ? (n == 0 ? byte_first : byte_next) : rows[n]);
    }
    ap_chunk(f, "IEND", NULL, 0u);
}

static SIXELSTATUS
ap_load(ap_fixture_t const *f, ap_probe_t *probe, int mode)
{
    sixel_allocator_t *allocator;
    sixel_loader_component_t *loader;
    sixel_chunk_t *chunk;
    loader_probe_callback_state_t callback;
    sixel_loader_suboptions_t options;
    sixel_loader_suboptions_t const *previous;
    SIXELSTATUS status;
    unsigned char background[3];
    int engine;
    int loops;
    int start;
    int previous_policy;

    allocator = NULL;
    loader = NULL;
    chunk = NULL;
    memset(background, 0, sizeof(background));
    memset(probe, 0, sizeof(*probe));
    probe->callback_status = mode == 9 ? SIXEL_BAD_INPUT : SIXEL_OK;
    callback = (loader_probe_callback_state_t){NULL, ap_capture, probe};
    loader_manager_init_loader_suboptions(&options);
    engine = mode <= 1 || mode == 7 || mode == 8 || mode == 11 || mode >= 14
           ? SIXEL_CMS_ENGINE_NONE : SIXEL_CMS_ENGINE_BUILTIN;
    options.builtin_enable_cms = engine != SIXEL_CMS_ENGINE_NONE;
    options.builtin_cms_engine = engine;
    options.cms_target_colorspace = SIXEL_COLORSPACE_LINEAR;
    options.cms_prefer_8bit = 0;
    previous = sixel_loader_activate_suboptions(&options);
    previous_policy = loader_transparent_policy();
    sixel_helper_set_loader_transparent_policy(
        SIXEL_LOADER_ALPHA_POLICY_COMPOSITE);
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status) || SIXEL_FAILED(create_loader_component_by_name(
            "builtin", allocator, (void **)&loader))) {
        goto end;
    }
    status = sixel_loader_component_setopt(loader,
        SIXEL_LOADER_COMPONENT_OPTION_CMS_ENGINE, &engine);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    loops = mode == 6 ? SIXEL_LOOP_AUTO : SIXEL_LOOP_DISABLE;
    status = sixel_loader_component_setopt(loader,
        SIXEL_LOADER_OPTION_LOOP_CONTROL, &loops);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (mode == 1 || mode == 12) {
        status = sixel_loader_component_setopt(loader,
            SIXEL_LOADER_OPTION_BGCOLOR, background);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }
    if (mode == 10) {
        start = 1;
        status = sixel_loader_component_setopt(loader,
            SIXEL_LOADER_OPTION_START_FRAME_NO, &start);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }
    status = sixel_chunk_create_from_memory(&chunk, f->bytes, f->size,
                                            NULL, allocator);
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_loader_component_load(loader, chunk,
            capture_frame_trampoline, &callback);
    }
end:
    if (chunk != NULL) {
        chunk->vtbl->unref(chunk);
    }
    sixel_loader_component_unref(loader);
    sixel_allocator_unref(allocator);
    sixel_loader_restore_suboptions(previous);
    sixel_helper_set_loader_transparent_policy(previous_policy);
    return status;
}

static int
ap_close(double actual, double expected)
{
    /* Includes sample normalization, transfer/matrix evaluation, the alpha
     * plane store and two composition stores. Not a bitwise FPU contract. */
    if (!isfinite(actual) || fabs(actual - expected) >
        1e-7 + 2e-6 * fabs(expected)) {
        fprintf(stderr, "APNG expected %.12g, got %.12g\n", expected, actual);
        return 0;
    }
    return 1;
}

int
test_apng_0001_float(int argc, char **argv)
{
    /* Display P3 D65 -> linear sRGB, independently solved primary matrices.
     * Pure primaries avoid dependence on the production TRC evaluator. */
    static double const primaries[3][3] = {
        {1.22494017628056, -0.04205695470969, -0.01963755459033},
        {-0.22494017628056, 1.04205695470969, -0.07863604555063},
        {0.0, 0.0, 1.098273600140966}
    };
    static unsigned const precise[6] = {1, 257, 32768, 32769, 65534, 65535};
    static unsigned const bytes[6] = {64, 128, 192, 48, 32, 16};
    ap_fixture_t f;
    ap_probe_t probe;
    SIXELSTATUS status;
    double expected;
    double sa;
    double da;
    double oa;
    double red;
    double green;
    int mode;
    int c;
    int n;
    int x;
    int frame;
    int count;

    if (argc != 2) {
        return EXIT_FAILURE;
    }
    mode = atoi(argv[1]);
    ap_build(&f, mode);
    status = ap_load(&f, &probe, mode);
    if (mode == 9) {
        return status == SIXEL_BAD_INPUT && probe.count == 1
             ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    count = mode == 16 ? 2 : mode == 17 ? 3
          : mode <= 1 || mode == 7 || mode >= 13 ? 1
          : mode == 6 ? 8 : mode == 8 ? 2 : mode == 10 ? 3 : 4;
    if (SIXEL_FAILED(status) || probe.count != count) {
        fprintf(stderr, "APNG status=%x frames=%d expected=%d: %s\n",
                status, probe.count, count,
                sixel_helper_get_additional_message());
        return EXIT_FAILURE;
    }
    sa = (double)(float)(16384.0 / 65535.0);
    da = (double)(float)(32768.0 / 65535.0);
    oa = sa + da * (1.0 - sa);
    for (n = 0; n < count; ++n) {
        frame = mode == 10 ? n + 1 : n % 4;
        if (probe.format[n] != (mode == 7 ||
                ((mode == 8 || mode >= 16) && n == 0)
                ? SIXEL_PIXELFORMAT_RGB888
                : SIXEL_PIXELFORMAT_LINEARRGBFLOAT32) ||
            probe.frame[n] != (mode == 10 ? n : frame) ||
            probe.loop[n] != n / 4) {
            fprintf(stderr, "APNG delivery %d format=%d frame=%d loop=%d\n",
                n, probe.format[n], probe.frame[n], probe.loop[n]);
            return EXIT_FAILURE;
        }
        /* Each wrapper owns one composition boundary; replay also checks
         * that the second pass starts from a cleared canvas. */
        if ((mode >= 2 && mode <= 5 && frame != mode - 2) ||
            (mode == 6 && n < 4)) {
            continue;
        }
        for (x = 0; x < 2; ++x) {
            if (probe.hidden[n][x] !=
                (mode <= 1 || mode == 12 || mode == 13 ? 0
                 : mode >= 16 ? mode == 17 && n == 2 && x == 1
                 : mode == 15 ? x == 0 : frame == 3 || (x == 1 && frame < 2))) {
                fprintf(stderr, "APNG mask frame=%d x=%d hidden=%d\n",
                        frame, x, probe.hidden[n][x]);
                return EXIT_FAILURE;
            }
            for (c = 0; c < 3; ++c) {
                red = mode == 11 ? (c == 0) : primaries[0][c];
                green = mode == 11 ? (c == 1) : primaries[1][c];
                expected = x == 0 ? red : green;
                if (frame == 1) {
                    expected = x == 0
                        ? (green * sa + red * da * (1.0 - sa)) / oa : 0.0;
                } else if (frame == 2 && x == 1) {
                    expected = mode == 11 ? (c == 2) : primaries[2][c];
                } else if (frame == 3) {
                    expected = 0.0;
                }
                if (mode >= 16) {
                    expected = c == 0;
                    if (x == 1 && n != 0) {
                        sa = (double)(float)(128.0 / 255.0);
                        expected = mode == 16
                            ? (c == 0) * (1.0 - sa) + (c == 2) * sa
                            : n == 1 ? (c == 2) : 0.0;
                    }
                } else if (mode == 14) {
                    expected = c == x;
                } else if (mode == 15) {
                    expected = ap_linear((c == 0 ? 1.0 : c == 1 ? 257.0
                        : x == 0 ? 32768.0 : 32769.0) / 65535.0);
                } else if (mode == 0 || mode == 13) {
                    expected = precise[x * 3 + c] / 65535.0;
                    if (mode == 0) {
                        expected = ap_linear(expected);
                    }
                } else if (mode == 1) {
                    expected = c == x ? (x == 0 ? 1.0 : 257.0) / 65535.0
                                      : 0.0;
                } else if (mode == 7 || mode == 8) {
                    expected = ap_linear(bytes[x * 3 + c] / 255.0);
                    if (mode == 8 && n == 1) {
                        sa = (double)(float)(128.0 / 255.0);
                        expected = x == 0
                            ? (c == 1) * sa + expected * (1.0 - sa) : 0.0;
                    }
                } else if (mode == 12) {
                    expected = frame == 0 && x == 0 ? red
                        : frame == 1 && x == 0
                        ? (green * sa + red * da * (1.0 - sa)) +
                            red * (1.0 - oa)
                        : frame == 2 && x == 1 ? primaries[2][c] : red;
                }
                if (!ap_close(probe.rgb[n][x * 3 + c], expected)) {
                    return EXIT_FAILURE;
                }
            }
        }
    }
    return EXIT_SUCCESS;
}
