/* Additional observations for the client linked against the release. */

static int
render_fixed_palette(unsigned char *source, int depth, sixel_dither_t *dither,
                     unsigned char **indices, unsigned char **palette,
                     int *colors)
{
    sixel_output_t *output = NULL;
    capture_t capture = {{0}, 0};
    int width = 0;
    int height = 0;
    SIXELSTATUS status;

    sixel_dither_set_diffusion_type(dither, SIXEL_DIFFUSE_NONE);
    if (sixel_output_new(&output, capture_sixel, &capture, NULL) != SIXEL_OK) {
        return 1;
    }
    sixel_output_set_palette_type(output, SIXEL_PALETTETYPE_RGB);
    status = sixel_encode(source, fixture_width, fixture_height,
                          depth, dither, output);
    sixel_output_unref(output);
    if (status != SIXEL_OK || capture.length == 0) {
        return 1;
    }
    status = sixel_decode_raw(capture.bytes, capture.length, indices, &width,
                              &height, palette, colors, NULL);
    return status != SIXEL_OK || width != fixture_width ||
           height != fixture_height || *indices == NULL || *palette == NULL;
}


static int
check_low_level(pixel_fixture_t const *fixture)
{
    unsigned char source[fixture_pixels * 4];
    unsigned char entries[sizeof(fixture_palette)];
    sixel_dither_t *dither = NULL;
    unsigned char *indices = NULL;
    unsigned char *palette = NULL;
    int count = palette_size(fixture);
    int colors = 0;
    int result = 1;

    /* Low-level gray and palette inputs address registers directly. For
     * truecolor, supply both source colors so no quantization is necessary.
     * The fixture covers row padding as well as channel and palette order. */
    make_source(fixture, source);
    if (fixture->format >= SIXEL_PIXELFORMAT_G1 &&
            fixture->format <= SIXEL_PIXELFORMAT_G8) {
        dither = sixel_dither_get(SIXEL_BUILTIN_G1 +
                                  fixture->format - SIXEL_PIXELFORMAT_G1);
        if (dither == NULL) {
            goto end;
        }
    } else {
        if (count > 0) {
            memcpy(entries, fixture_palette, sizeof(entries));
        } else {
            count = 2;
            memcpy(entries, fixture->normalized, 6);
        }
        if (sixel_dither_new(&dither, count, NULL) != SIXEL_OK) {
            goto end;
        }
        sixel_dither_set_palette(dither, entries);
    }
    sixel_dither_set_pixelformat(dither, fixture->format);
    if (render_fixed_palette(source, (fixture->bits_per_pixel + 7) / 8,
            dither, &indices, &palette, &colors) == 0 &&
            first_wrong_pixel(fixture, indices, palette, colors)
                == fixture_pixels) {
        result = 0;
    }

end:
    if (dither != NULL) {
        sixel_dither_unref(dither);
    }
    free(indices);
    free(palette);
    return result;
}

static int
check_complexion(int historical)
{
    unsigned char entries[] = {120, 80, 0, 90, 130, 0};
    unsigned char const sample[] = {90, 80, 0};
    unsigned char const colors_rgb[][3] = {{120, 79, 0}, {89, 130, 0}};
    unsigned char source[fixture_pixels * 3];
    unsigned char *indices;
    unsigned char *palette;
    unsigned char *rgb;
    sixel_dither_t *dither;
    int colors;
    int selected = -1;
    int result = 0;
    int current;
    int score;
    int i;

    /* For (90,80,0) the two squared distances are 900*score and 2500.
     * A current implementation may improve the default distance metric.
     * The deprecated setter must still leave that choice alone. */
    for (score = 1; score <= 4; score += 3) {
        for (i = 0; i < fixture_pixels; ++i) {
            memcpy(source + 3 * i, sample, sizeof(sample));
        }
        indices = NULL;
        palette = NULL;
        colors = 0;
        if (sixel_dither_new(&dither, 2, NULL) != SIXEL_OK) {
            return 1;
        }
        sixel_dither_set_palette(dither, entries);
        sixel_dither_set_pixelformat(dither, SIXEL_PIXELFORMAT_RGB888);
        sixel_dither_set_complexion_score(dither, score);
        result = render_fixed_palette(source, 3, dither, &indices, &palette,
                                      &colors);
        sixel_dither_unref(dither);
        for (i = 0; result == 0 && i < fixture_pixels; ++i) {
            if (indices[i] >= colors) {
                result = 1;
                break;
            }
            rgb = palette + 3 * indices[i];
            current = memcmp(rgb, colors_rgb[0], 3) == 0 ? 0 : 1;
            if (selected < 0) {
                selected = current;
            }
            if (memcmp(rgb, colors_rgb[current], 3) != 0 ||
                    current != (historical ? (score == 4) : selected)) {
                result = 1;
            }
        }
        free(palette);
        free(indices);
        if (result != 0) {
            break;
        }
    }
    return result;
}


static int
check_scale(int historical)
{
    /* Two source pixels become pixels 0, 0, 1. Keep room for all nine RGB
     * bytes on BOTH versions. The tail shows how much each library wrote. */
    static struct {
        int format;
        unsigned char source[6];
        unsigned char release[9];
        unsigned char rgb[9];
    } const cases[] = {
        {SIXEL_PIXELFORMAT_G8, {36, 202},
         {36, 36, 36, 238, 238, 238, 238, 238, 238},
         {36, 36, 36, 36, 36, 36, 202, 202, 202}},
        {SIXEL_PIXELFORMAT_RGB565, {0x12, 0x9a, 0xd1, 0x0d},
         {16, 80, 16, 80, 208, 208, 238, 238, 238},
         {16, 80, 208, 16, 80, 208, 208, 32, 104}},
        {SIXEL_PIXELFORMAT_RGB888, {16, 80, 208, 208, 32, 104},
         {16, 80, 208, 16, 80, 208, 208, 32, 104},
         {16, 80, 208, 16, 80, 208, 208, 32, 104}}
    };
    sixel_allocator_t *allocator = NULL;
    unsigned char dst[10];
    unsigned char const *expected;
    size_t i;
    int byte;
    int tolerance;
    int result = 0;
    SIXELSTATUS status;

    if (sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL) != SIXEL_OK) {
        return 1;
    }
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        memset(dst, 238, sizeof(dst));
        status = sixel_helper_scale_image(dst, cases[i].source, 2, 1,
            cases[i].format, 3, 1, SIXEL_RES_NEAREST, allocator);
        expected = historical ? cases[i].release : cases[i].rgb;
        for (byte = 0; byte < 9; ++byte) {
            /* Full-range expansion of 5/6 bit channels may fill low bits. */
            tolerance = 0;
            if (!historical && cases[i].format == SIXEL_PIXELFORMAT_RGB565) {
                tolerance = byte % 3 == 1 ? 3 : 7;
            }
            if (abs(dst[byte] - expected[byte]) > tolerance) {
                break;
            }
        }
        if (status != SIXEL_OK || byte != 9 || dst[9] != 238) {
            fprintf(stderr, "scale: format 0x%x\n", cases[i].format);
            result = 1;
        }
    }
    sixel_allocator_unref(allocator);
    return result;
}


static int
check_registers(void)
{
    /* Independent DEC wire fixture. HLS (150,45,37) gives percentages
     * (61.65,45,28.35). The other two registers are exact controls. */
    unsigned char wire[] = "\033Pq\"1;1;3;1#1;1;150;45;37@"
        "#2;1;120;50;100@#3;2;20;40;80@\033\\";
    int const expected[] = {157, 115, 72, 255, 0, 0, 51, 102, 204};
    unsigned char *indices = NULL;
    unsigned char *palette = NULL;
    int width = 0;
    int height = 0;
    int colors = 0;
    int channel;
    int i;
    int result = 1;
    SIXELSTATUS status;

    status = sixel_decode_raw(wire, (int)sizeof(wire) - 1, &indices, &width,
                              &height, &palette, &colors, NULL);
    if (status == SIXEL_OK && width == 3 && height == 1 &&
            colors >= 3 && indices != NULL && palette != NULL) {
        result = 0;
        for (i = 0; i < 3; ++i) {
            if (indices[i] >= colors) {
                result = 1;
                break;
            }
            for (channel = 0; channel < 3; ++channel) {
                /* One integral wire percentage covers rounding either way. */
                if (abs(palette[3 * indices[i] + channel] -
                        expected[3 * i + channel]) > (i == 0 ? 3 : 0)) {
                    fprintf(stderr, "decode: register %d channel %d\n",
                            i + 1, channel);
                    result = 1;
                }
            }
        }
    }
    free(palette);
    free(indices);
    return result;
}


static int
check_builtin_palettes(void)
{
    static int const counts[] = {2, 2, 16, 256, 16, 16, 2, 4, 16, 256};
    sixel_dither_t *dither;
    unsigned char const *rgb;
    int id;
    int n;
    int i;
    int result = 0;

    for (id = SIXEL_BUILTIN_MONO_DARK; id <= SIXEL_BUILTIN_G8; ++id) {
        dither = sixel_dither_get(id);
        if (dither == NULL) {
            return 1;
        }
        n = counts[id];
        rgb = sixel_dither_get_palette(dither);
        if (rgb == NULL || sixel_dither_get_num_of_palette_colors(dither)
                != n) {
            result = 1;
        } else if (id >= SIXEL_BUILTIN_G1) {
            /* Every published shade must remain usable, in luminance order. */
            for (i = 0; i < n; ++i) {
                if (rgb[3 * i] != rgb[3 * i + 1] ||
                        rgb[3 * i] != rgb[3 * i + 2] ||
                        (i > 0 && rgb[3 * i] <= rgb[3 * (i - 1)])) {
                    result = 1;
                }
            }
            if (rgb[0] != 0 || rgb[3 * (n - 1)] != 255) {
                result = 1;
            }
        } else if (id <= SIXEL_BUILTIN_MONO_LIGHT) {
            if (rgb[0] != (id == SIXEL_BUILTIN_MONO_DARK ? 0 : 255) ||
                    rgb[3] != 255 - rgb[0]) {
                result = 1;
            }
        }
        sixel_dither_unref(dither);
    }
    return result;
}


/* Only the allocator object itself needs storage. During the boundary
 * probes each callback records the request and immediately returns NULL. */
static int recording;
static size_t observed_size;

static void *
observe_malloc(size_t n)
{
    if (!recording) {
        return malloc(n);
    }
    observed_size = n;
    return NULL;
}

static void *
observe_calloc(size_t n, size_t size)
{
    observed_size = n * size;
    return NULL;
}

static void *
observe_realloc(void *p, size_t size)
{
    (void)p;
    observed_size = size;
    return NULL;
}

static int
check_allocator_limit(int historical)
{
    static size_t const sizes[] = {
        134217728, 134217729, 1343225856, 1343225857
    };
    size_t limit = historical ? 1343225856UL : 134217728UL;
    sixel_allocator_t *allocator = NULL;
    size_t i;
    size_t expected;
    void *p;
    int op;
    int result = 0;

    if (sixel_allocator_new(&allocator, observe_malloc, observe_calloc,
                            observe_realloc, free) != SIXEL_OK) {
        return 1;
    }
    recording = 1;
    for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
        expected = sizes[i] <= limit ? sizes[i] : 0;
        for (op = 0; op < 3; ++op) {
            observed_size = 0;
            if (op == 0) {
                p = sixel_allocator_malloc(allocator, sizes[i]);
            } else if (op == 1) {
                p = sixel_allocator_calloc(allocator, sizes[i], 1);
            } else {
                p = sixel_allocator_realloc(allocator, NULL, sizes[i]);
            }
            if (p != NULL || observed_size != expected) {
                fprintf(stderr, "allocator: op=%d size=%zu callback=%zu\n",
                        op, sizes[i], observed_size);
                result = 1;
            }
        }
    }
    recording = 0;
    sixel_allocator_unref(allocator);
    return result;
}


static int
check_option_choices(int historical)
{
    static struct {
        int flag;
        char const *values[11];
    } const groups[] = {
        {SIXEL_OPTFLAG_DIFFUSION, {"auto", "none", "fs", "atkinson", "jajuni",
            "stucki", "burkes", "a_dither", "x_dither"}},
        {SIXEL_OPTFLAG_FIND_LARGEST, {"auto", "norm", "lum"}},
        {SIXEL_OPTFLAG_SELECT_COLOR, {"auto", "center", "average", "histogram",
            "histgram"}},
        {SIXEL_OPTFLAG_RESAMPLING, {"nearest", "gaussian", "hanning",
            "hamming", "bilinear", "welsh", "bicubic", "lanczos2", "lanczos3",
            "lanczos4"}},
        {SIXEL_OPTFLAG_WIDTH, {"12", "12px", "50%", "auto"}},
        {SIXEL_OPTFLAG_HEIGHT, {"12", "12px", "50%", "auto"}},
        {SIXEL_OPTFLAG_CROP, {"7x3+2+1"}},
        {SIXEL_OPTFLAG_BGCOLOR, {"#123456", "rgb:11/88/ee"}},
        {SIXEL_OPTFLAG_QUALITY, {"auto", "high", "low", "full"}},
        {SIXEL_OPTFLAG_LOOPMODE, {"auto", "force", "disable"}},
        {SIXEL_OPTFLAG_PALETTE_TYPE, {"auto", "hls", "rgb"}},
        {SIXEL_OPTFLAG_ENCODE_POLICY, {"auto", "fast", "size"}},
        {SIXEL_OPTFLAG_BUILTIN_PALETTE, {"xterm16", "xterm256", "vt340mono",
            "vt340color", "gray1", "gray2", "gray4", "gray8"}}
    };
    sixel_encoder_t *encoder = NULL;
    char const *value;
    size_t i;
    int j;
    int result = 0;
    SIXELSTATUS status;
    SIXELSTATUS expected;

    if (sixel_encoder_new(&encoder, NULL) != SIXEL_OK) {
        return probe_fail;
    }
    for (i = 0; i < sizeof(groups) / sizeof(groups[0]); ++i) {
        for (j = 0; groups[i].values[j] != NULL; ++j) {
            value = groups[i].values[j];
            status = sixel_encoder_setopt(encoder, groups[i].flag, value);
            /* The misspelling was retired. A client using the documented
             * spelling must still succeed on every library. */
            expected = !historical && strcmp(value, "histgram") == 0
                     ? SIXEL_BAD_ARGUMENT : SIXEL_OK;
            if (status != expected) {
                fprintf(stderr, "setopt -%c %s: 0x%x, expected 0x%x\n",
                        groups[i].flag, value, (unsigned)status,
                        (unsigned)expected);
                result = 1;
            }
        }
    }
    sixel_encoder_unref(encoder);
    return result;
}


static int
check_selector_order(int historical)
{
    static int const flags[] = {
        SIXEL_OPTFLAG_MONOCHROME, SIXEL_OPTFLAG_BUILTIN_PALETTE
    };
    static char const *const values[] = {NULL, "gray2"};
    sixel_encoder_t *encoder;
    SIXELSTATUS expected = historical ? SIXEL_OK : SIXEL_BAD_ARGUMENT;
    int i;
    int last;
    int result = 0;

    for (i = 0; i < 2; ++i) {
        if (sixel_encoder_new(&encoder, NULL) != SIXEL_OK) {
            return 1;
        }
        last = historical ? 1 - i : i;
        if (sixel_encoder_setopt(encoder, flags[i], values[i]) != SIXEL_OK ||
                sixel_encoder_setopt(encoder, flags[1 - i], values[1 - i])
                    != expected ||
                sixel_encoder_setopt(encoder, flags[last], values[last])
                    != SIXEL_OK) {
            fprintf(stderr, "palette selector order beginning -%c\n", flags[i]);
            result = 1;
        }
        sixel_encoder_unref(encoder);
    }
    return result;
}
