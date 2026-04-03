/*
 * Verify CoreGraphics output policy for alpha, indexed, and high-depth paths.
 */

#include "tests/loader/pixelformat_test_common.h"

#if HAVE_COREGRAPHICS
static SIXELSTATUS
new_coregraphics_component(sixel_allocator_t *allocator,
                           sixel_loader_component_t **ppcomponent)
{
    return create_loader_component_by_name("coregraphics",
                                           allocator,
                                           ppcomponent);
}

typedef struct coregraphics_animation_probe {
    int callback_count;
    int first_delay;
    int second_delay;
    int first_multiframe;
    int second_multiframe;
    int first_rgb[3];
    int has_first_rgb;
} coregraphics_animation_probe_t;

#define WEBP_LOOP2_IMAGE_PATH \
    "/tests/data/inputs/formats/animated-lossless-8x8-2frame-loop2-min.webp"

typedef struct coregraphics_loop_sequence_probe {
    int callback_count;
    int expected_count;
    int mismatch_index;
    int mismatch_loop_no;
    int mismatch_frame_no;
    int saw_multiframe;
    int const *expected_sequence;
} coregraphics_loop_sequence_probe_t;

typedef struct coregraphics_loop_unbounded_probe {
    int callback_count;
    int max_callbacks;
    int required_loop_no;
    int highest_loop_no;
    int highest_frame_no;
    int saw_multiframe;
    int reached_required_loop;
} coregraphics_loop_unbounded_probe_t;

static int
coregraphics_runtime_major_version(void)
{
    char const *version_text;
    char *endptr;
    long major;

    version_text = NULL;
    endptr = NULL;
    major = 0;
    version_text = getenv("SIXEL_TEST_MACOS_PRODUCT_VERSION");
    if (version_text == NULL || version_text[0] == '\0') {
        return 0;
    }

    major = strtol(version_text, &endptr, 10);
    if (endptr == version_text || major <= 0 || major > INT_MAX) {
        return 0;
    }
    return (int)major;
}

static int
coregraphics_runtime_supports_webp_animation(void)
{
    int major;

    major = coregraphics_runtime_major_version();
    if (major == 0) {
        return 1;
    }
    if (major >= 11) {
        return 1;
    }
    return 0;
}

static int
coregraphics_capture_first_rgb_sample(sixel_frame_t *frame, int rgb[3])
{
    int pixelformat;
    unsigned char const *pixels_u8;
    float const *pixels_f32;
    int channel;
    float channel_value;

    pixelformat = 0;
    pixels_u8 = NULL;
    pixels_f32 = NULL;
    channel = 0;
    channel_value = 0.0f;
    if (frame == NULL || rgb == NULL) {
        return 0;
    }

    pixelformat = sixel_frame_get_pixelformat(frame);
    switch (pixelformat) {
    case SIXEL_PIXELFORMAT_RGB888:
        pixels_u8 = sixel_frame_get_pixels(frame);
        if (pixels_u8 == NULL) {
            return 0;
        }
        rgb[0] = pixels_u8[0];
        rgb[1] = pixels_u8[1];
        rgb[2] = pixels_u8[2];
        return 1;
    case SIXEL_PIXELFORMAT_RGBFLOAT32:
    case SIXEL_PIXELFORMAT_LINEARRGBFLOAT32:
        pixels_f32 = sixel_frame_get_pixels_float32(frame);
        if (pixels_f32 == NULL) {
            return 0;
        }
        for (channel = 0; channel < 3; ++channel) {
            channel_value = pixels_f32[channel];
            if (channel_value < 0.0f) {
                channel_value = 0.0f;
            } else if (channel_value > 1.0f) {
                channel_value = 1.0f;
            }
            rgb[channel] = (int)(channel_value * 255.0f + 0.5f);
        }
        return 1;
    default:
        break;
    }

    return 0;
}

static SIXELSTATUS
capture_coregraphics_animation_probe(sixel_frame_t *frame, void *data)
{
    coregraphics_animation_probe_t *probe;
    int rgb[3];

    probe = NULL;
    rgb[0] = 0;
    rgb[1] = 0;
    rgb[2] = 0;
    if (frame == NULL || data == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    probe = (coregraphics_animation_probe_t *)data;
    if (probe->callback_count == 0) {
        probe->first_delay = sixel_frame_get_delay(frame);
        probe->first_multiframe = sixel_frame_get_multiframe(frame);
        probe->has_first_rgb = coregraphics_capture_first_rgb_sample(
            frame,
            rgb);
        if (probe->has_first_rgb) {
            probe->first_rgb[0] = rgb[0];
            probe->first_rgb[1] = rgb[1];
            probe->first_rgb[2] = rgb[2];
        }
    } else if (probe->callback_count == 1) {
        probe->second_delay = sixel_frame_get_delay(frame);
        probe->second_multiframe = sixel_frame_get_multiframe(frame);
    }

    probe->callback_count += 1;
    return SIXEL_OK;
}

static int
run_coregraphics_animation_case_with_callback(
    char const *label,
    char const *relative_path,
    int loop_control,
    int start_frame_no_set,
    int start_frame_no,
    sixel_load_image_function callback,
    void *callback_context,
    SIXELSTATUS *out_status)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_chunk_t *chunk;
    sixel_loader_component_t *component;
    char const *source_root;
    char image_path[PATH_MAX];
    int cancel_flag;
    int require_static;
    int use_palette;
    int reqcolors;
    loader_probe_callback_state_t callback_state;

    status = SIXEL_FALSE;
    allocator = NULL;
    chunk = NULL;
    component = NULL;
    source_root = NULL;
    cancel_flag = 0;
    require_static = 0;
    use_palette = 0;
    reqcolors = 256;
    callback_state.loader = NULL;
    callback_state.fn = NULL;
    callback_state.context = NULL;
    if (label == NULL ||
        relative_path == NULL ||
        callback == NULL ||
        callback_context == NULL) {
        return 1;
    }
    if (out_status != NULL) {
        *out_status = SIXEL_FALSE;
    }

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
                         relative_path,
                         image_path,
                         sizeof(image_path)) != 0) {
        fprintf(stderr, "%s: failed to build image path\n", label);
        return 1;
    }

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: allocator initialization failed\n", label);
        return 1;
    }

    status = sixel_chunk_new(&chunk,
                             image_path,
                             0,
                             &cancel_flag,
                             allocator);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: failed to read sample\n", label);
        goto cleanup;
    }

    status = new_coregraphics_component(allocator, &component);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: component init failed (%d)\n", label, (int)status);
        goto cleanup;
    }

    status = sixel_loader_component_setopt(component,
                                           SIXEL_LOADER_OPTION_REQUIRE_STATIC,
                                           &require_static);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_loader_component_setopt(component,
                                           SIXEL_LOADER_OPTION_USE_PALETTE,
                                           &use_palette);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_loader_component_setopt(component,
                                           SIXEL_LOADER_OPTION_REQCOLORS,
                                           &reqcolors);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_loader_component_setopt(component,
                                           SIXEL_LOADER_OPTION_LOOP_CONTROL,
                                           &loop_control);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    if (start_frame_no_set != 0) {
        status = sixel_loader_component_setopt(
            component,
            SIXEL_LOADER_OPTION_START_FRAME_NO,
            &start_frame_no);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
    }

    callback_state.loader = NULL;
    callback_state.fn = callback;
    callback_state.context = callback_context;

    status = sixel_loader_component_load(component,
                                         chunk,
                                         capture_frame_trampoline,
                                         &callback_state);
    if (out_status != NULL) {
        *out_status = status;
    }
    status = SIXEL_OK;

cleanup:
    sixel_loader_component_unref(component);
    sixel_chunk_destroy(chunk);
    sixel_allocator_unref(allocator);

    return SIXEL_FAILED(status) ? 1 : 0;
}

static int
run_coregraphics_animation_case(char const *label,
                                char const *relative_path,
                                int loop_control,
                                int start_frame_no_set,
                                int start_frame_no,
                                coregraphics_animation_probe_t *probe)
{
    SIXELSTATUS status;
    int result;

    status = SIXEL_FALSE;
    result = 1;
    if (probe == NULL) {
        return 1;
    }

    probe->callback_count = 0;
    probe->first_delay = 0;
    probe->second_delay = 0;
    probe->first_multiframe = 0;
    probe->second_multiframe = 0;
    probe->first_rgb[0] = 0;
    probe->first_rgb[1] = 0;
    probe->first_rgb[2] = 0;
    probe->has_first_rgb = 0;

    result = run_coregraphics_animation_case_with_callback(
        label,
        relative_path,
        loop_control,
        start_frame_no_set,
        start_frame_no,
        capture_coregraphics_animation_probe,
        probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "%s: loader reported failure (%d)\n",
                label,
                (int)status);
        return 1;
    }

    return 0;
}

static SIXELSTATUS
capture_coregraphics_sequence_probe(sixel_frame_t *frame, void *data)
{
    coregraphics_loop_sequence_probe_t *probe;
    int loop_no;
    int frame_no;
    int expected_index;
    int expected_loop_no;
    int expected_frame_no;

    probe = NULL;
    loop_no = 0;
    frame_no = 0;
    expected_index = 0;
    expected_loop_no = 0;
    expected_frame_no = 0;
    if (frame == NULL || data == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    probe = (coregraphics_loop_sequence_probe_t *)data;
    if (probe->expected_sequence == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    loop_no = sixel_frame_get_loop_no(frame);
    frame_no = sixel_frame_get_frame_no(frame);
    if (sixel_frame_get_multiframe(frame) != 0) {
        probe->saw_multiframe = 1;
    }

    if (probe->callback_count >= probe->expected_count) {
        probe->mismatch_index = probe->callback_count;
        probe->mismatch_loop_no = loop_no;
        probe->mismatch_frame_no = frame_no;
        return SIXEL_BAD_INPUT;
    }

    expected_index = probe->callback_count * 2;
    expected_loop_no = probe->expected_sequence[expected_index + 0];
    expected_frame_no = probe->expected_sequence[expected_index + 1];
    if (loop_no != expected_loop_no || frame_no != expected_frame_no) {
        probe->mismatch_index = probe->callback_count;
        probe->mismatch_loop_no = loop_no;
        probe->mismatch_frame_no = frame_no;
        return SIXEL_BAD_INPUT;
    }

    probe->callback_count += 1;
    return SIXEL_OK;
}

static SIXELSTATUS
capture_coregraphics_loop_probe_until_target(sixel_frame_t *frame, void *data)
{
    coregraphics_loop_unbounded_probe_t *probe;
    int loop_no;
    int frame_no;

    probe = NULL;
    loop_no = 0;
    frame_no = 0;
    if (frame == NULL || data == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    probe = (coregraphics_loop_unbounded_probe_t *)data;
    loop_no = sixel_frame_get_loop_no(frame);
    frame_no = sixel_frame_get_frame_no(frame);
    probe->callback_count += 1;
    if (loop_no > probe->highest_loop_no) {
        probe->highest_loop_no = loop_no;
    }
    if (frame_no > probe->highest_frame_no) {
        probe->highest_frame_no = frame_no;
    }
    if (sixel_frame_get_multiframe(frame) != 0) {
        probe->saw_multiframe = 1;
    }

    /*
     * Force-loop mode is intentionally unbounded, so interrupt once the
     * required loop index is observed. The callback limit guards regressions.
     */
    if (loop_no >= probe->required_loop_no) {
        probe->reached_required_loop = 1;
        return SIXEL_INTERRUPTED;
    }
    if (probe->callback_count >= probe->max_callbacks) {
        return SIXEL_INTERRUPTED;
    }

    return SIXEL_OK;
}

static int
run_coregraphics_loop_sequence_case(char const *label,
                                    char const *relative_path,
                                    int loop_control,
                                    int const *expected_sequence,
                                    int expected_count)
{
    SIXELSTATUS status;
    coregraphics_loop_sequence_probe_t probe;
    int mismatch_expected_loop_no;
    int mismatch_expected_frame_no;
    int result;

    status = SIXEL_FALSE;
    probe.callback_count = 0;
    probe.expected_count = expected_count;
    probe.mismatch_index = -1;
    probe.mismatch_loop_no = -1;
    probe.mismatch_frame_no = -1;
    probe.saw_multiframe = 0;
    probe.expected_sequence = expected_sequence;
    mismatch_expected_loop_no = -1;
    mismatch_expected_frame_no = -1;
    result = 1;
    if (label == NULL ||
        relative_path == NULL ||
        expected_sequence == NULL ||
        expected_count <= 0) {
        return 1;
    }

    result = run_coregraphics_animation_case_with_callback(
        label,
        relative_path,
        loop_control,
        0,
        INT_MIN,
        capture_coregraphics_sequence_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }

    if (status != SIXEL_OK) {
        if (status == SIXEL_BAD_INPUT &&
            probe.mismatch_index >= 0 &&
            probe.mismatch_index < expected_count) {
            mismatch_expected_loop_no =
                expected_sequence[probe.mismatch_index * 2 + 0];
            mismatch_expected_frame_no =
                expected_sequence[probe.mismatch_index * 2 + 1];
            fprintf(stderr,
                    "%s: sequence mismatch at %d "
                    "(actual=%d:%d expected=%d:%d)\n",
                    label,
                    probe.mismatch_index,
                    probe.mismatch_loop_no,
                    probe.mismatch_frame_no,
                    mismatch_expected_loop_no,
                    mismatch_expected_frame_no);
        } else {
            fprintf(stderr,
                    "%s: loader reported failure (%d)\n",
                    label,
                    (int)status);
        }
        return 1;
    }
    if (probe.callback_count != expected_count) {
        fprintf(stderr,
                "%s: callback count mismatch (actual=%d expected=%d)\n",
                label,
                probe.callback_count,
                expected_count);
        return 1;
    }
    if (probe.saw_multiframe == 0) {
        fprintf(stderr, "%s: frame metadata did not mark multiframe\n", label);
        return 1;
    }

    return 0;
}

static int
run_coregraphics_loop_force_unbounded_case(char const *label,
                                           char const *relative_path)
{
    SIXELSTATUS status;
    coregraphics_loop_unbounded_probe_t probe;
    int result;

    status = SIXEL_FALSE;
    probe.callback_count = 0;
    probe.max_callbacks = 64;
    probe.required_loop_no = 2;
    probe.highest_loop_no = -1;
    probe.highest_frame_no = -1;
    probe.saw_multiframe = 0;
    probe.reached_required_loop = 0;
    result = 1;
    if (label == NULL || relative_path == NULL) {
        return 1;
    }

    result = run_coregraphics_animation_case_with_callback(
        label,
        relative_path,
        SIXEL_LOOP_FORCE,
        0,
        INT_MIN,
        capture_coregraphics_loop_probe_until_target,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }

    if (status != SIXEL_INTERRUPTED) {
        fprintf(stderr,
                "%s: expected interruption status, got %d\n",
                label,
                (int)status);
        return 1;
    }
    if (probe.reached_required_loop == 0) {
        fprintf(stderr,
                "%s: loop threshold not reached "
                "(required=%d highest=%d callbacks=%d)\n",
                label,
                probe.required_loop_no,
                probe.highest_loop_no,
                probe.callback_count);
        return 1;
    }
    if (probe.saw_multiframe == 0) {
        fprintf(stderr, "%s: frame metadata did not mark multiframe\n", label);
        return 1;
    }

    return 0;
}

static int
run_coregraphics_loop_control_case(char const *label,
                                   char const *relative_path)
{
    static int const expected_once[] = { 0, 0, 0, 1 };
    static int const expected_twice[] = { 0, 0, 0, 1, 1, 0, 1, 1 };
    int result;

    result = 1;
    if (label == NULL || relative_path == NULL) {
        return 1;
    }

    result = run_coregraphics_loop_sequence_case(label,
                                                 relative_path,
                                                 SIXEL_LOOP_DISABLE,
                                                 expected_once,
                                                 2);
    if (result != 0) {
        return result;
    }

    result = run_coregraphics_loop_sequence_case(label,
                                                 relative_path,
                                                 SIXEL_LOOP_AUTO,
                                                 expected_twice,
                                                 4);
    if (result != 0) {
        return result;
    }

    result = run_coregraphics_loop_force_unbounded_case(label, relative_path);
    if (result != 0) {
        return result;
    }

    return 0;
}

static int
run_coregraphics_animation_metadata_case(char const *label,
                                         char const *relative_path)
{
    coregraphics_animation_probe_t probe;
    int result;

    probe.callback_count = 0;
    probe.first_delay = 0;
    probe.second_delay = 0;
    probe.first_multiframe = 0;
    probe.second_multiframe = 0;
    probe.first_rgb[0] = 0;
    probe.first_rgb[1] = 0;
    probe.first_rgb[2] = 0;
    probe.has_first_rgb = 0;

    result = run_coregraphics_animation_case(label,
                                             relative_path,
                                             SIXEL_LOOP_DISABLE,
                                             0,
                                             INT_MIN,
                                             &probe);
    if (result != 0) {
        return result;
    }

    if (probe.callback_count != 2) {
        fprintf(stderr,
                "%s: expected callback count 2, got %d\n",
                label,
                probe.callback_count);
        return 1;
    }
    if (probe.first_multiframe != 1 || probe.second_multiframe != 1) {
        fprintf(stderr,
                "%s: expected multiframe metadata on animated frames\n",
                label);
        return 1;
    }
    if (probe.first_delay <= 0 || probe.second_delay <= 0) {
        fprintf(stderr, "%s: expected positive frame delay metadata\n", label);
        return 1;
    }

    return 0;
}

static int
run_coregraphics_start_frame_case(char const *label,
                                  char const *relative_path)
{
    coregraphics_animation_probe_t baseline;
    coregraphics_animation_probe_t positive;
    coregraphics_animation_probe_t negative;
    int result;

    baseline.callback_count = 0;
    positive.callback_count = 0;
    negative.callback_count = 0;
    result = run_coregraphics_animation_case(label,
                                             relative_path,
                                             SIXEL_LOOP_DISABLE,
                                             0,
                                             INT_MIN,
                                             &baseline);
    if (result != 0) {
        return result;
    }

    result = run_coregraphics_animation_case(label,
                                             relative_path,
                                             SIXEL_LOOP_DISABLE,
                                             1,
                                             1,
                                             &positive);
    if (result != 0) {
        return result;
    }

    result = run_coregraphics_animation_case(label,
                                             relative_path,
                                             SIXEL_LOOP_DISABLE,
                                             1,
                                             -1,
                                             &negative);
    if (result != 0) {
        return result;
    }

    if (!baseline.has_first_rgb || !positive.has_first_rgb ||
        !negative.has_first_rgb) {
        fprintf(stderr,
                "%s: failed to capture first-frame RGB sample\n",
                label);
        return 1;
    }
    if (baseline.first_rgb[0] == positive.first_rgb[0] &&
        baseline.first_rgb[1] == positive.first_rgb[1] &&
        baseline.first_rgb[2] == positive.first_rgb[2]) {
        fprintf(stderr,
                "%s: positive start frame did not change first frame\n",
                label);
        return 1;
    }
    if (negative.first_rgb[0] != positive.first_rgb[0] ||
        negative.first_rgb[1] != positive.first_rgb[1] ||
        negative.first_rgb[2] != positive.first_rgb[2]) {
        fprintf(stderr,
                "%s: negative start frame did not map to final frame\n",
                label);
        return 1;
    }

    return 0;
}

static int
run_coregraphics_loader_test(void)
{
    static unsigned char const white_bg[3] = { 255u, 255u, 255u };
    loader_component_case_spec_t const specs[] = {
        {
            "coregraphics rgba no background emits rgb+mask",
            RGBA_IMAGE_PATH,
            {
                SIXEL_PIXELFORMAT_RGB888,
                2,
                1,
                1,
                -1,
                FRAME_METADATA_ANY,
                1,
                1
            },
            { 1, 0, 256, NULL },
            new_coregraphics_component
        },
        {
            "coregraphics rgba with background emits rgb",
            RGBA_IMAGE_PATH,
            {
                SIXEL_PIXELFORMAT_RGB888,
                2,
                1,
                1,
                -1,
                FRAME_METADATA_ANY,
                0,
                0
            },
            { 1, 0, 256, white_bg },
            new_coregraphics_component
        },
        {
            "coregraphics indexed png keeps pal8",
            "/tests/data/inputs/formats/snake-png-pal8.png",
            {
                SIXEL_PIXELFORMAT_PAL8,
                64,
                64,
                1,
                -1,
                FRAME_METADATA_ANY,
                0,
                0
            },
            { 1, 1, 256, NULL },
            new_coregraphics_component
        },
        {
            "coregraphics indexed keycolor keeps pal8+transparent",
            "/tests/data/inputs/formats/pal8-trns-key0.png",
            {
                SIXEL_PIXELFORMAT_PAL8,
                4,
                1,
                1,
                FRAME_TRANSPARENT_NONNEG,
                FRAME_METADATA_ANY,
                0,
                0
            },
            { 1, 1, 256, NULL },
            new_coregraphics_component
        },
        {
            "coregraphics indexed keycolor icc/gama keeps pal8+transparent",
            "/tests/data/inputs/formats/pal8-trns-key0-gama-icc.png",
            {
                SIXEL_PIXELFORMAT_PAL8,
                4,
                1,
                1,
                FRAME_TRANSPARENT_NONNEG,
                FRAME_METADATA_ANY,
                0,
                0
            },
            { 1, 1, 256, NULL },
            new_coregraphics_component
        },
        {
            "coregraphics indexed keycolor reqcolors fallback rgb+mask",
            "/tests/data/inputs/formats/pal8-trns-key0.png",
            {
                SIXEL_PIXELFORMAT_RGB888,
                4,
                1,
                1,
                -1,
                FRAME_METADATA_ANY,
                1,
                1
            },
            { 1, 1, 3, NULL },
            new_coregraphics_component
        },
        {
            "coregraphics indexed reqcolors fallback emits rgb",
            "/tests/data/inputs/formats/snake-png-pal8.png",
            {
                SIXEL_PIXELFORMAT_RGB888,
                64,
                64,
                1,
                -1,
                FRAME_METADATA_ANY,
                0,
                0
            },
            { 1, 1, 253, NULL },
            new_coregraphics_component
        },
        {
            "coregraphics indexed alpha fallback emits rgb+mask",
            "/tests/data/inputs/formats/libpng-pal8-trns-single0-semi-icc.png",
            {
                SIXEL_PIXELFORMAT_RGB888,
                6,
                1,
                1,
                -1,
                FRAME_METADATA_ANY,
                1,
                1
            },
            { 1, 1, 256, NULL },
            new_coregraphics_component
        },
        {
            "coregraphics indexed alpha with background composites",
            "/tests/data/inputs/formats/libpng-pal8-trns-single0-semi-icc.png",
            {
                SIXEL_PIXELFORMAT_RGB888,
                6,
                1,
                1,
                -1,
                FRAME_METADATA_ANY,
                0,
                0
            },
            { 1, 1, 256, white_bg },
            new_coregraphics_component
        },
        {
            "coregraphics high-depth png promotes to float32",
            "/tests/data/inputs/formats/snake-png-gray16.png",
            {
                SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
                64,
                64,
                1,
                -1,
                FRAME_METADATA_ANY,
                0,
                0
            },
            { 1, 0, 256, NULL },
            new_coregraphics_component
        }
    };
    size_t index;
    int result;

    index = 0u;
    result = 0;
    for (index = 0u; index < sizeof(specs) / sizeof(specs[0]); ++index) {
        result = run_loader_component_case_from_spec(&specs[index]);
        if (result != 0) {
            return result;
        }
    }

    result = run_coregraphics_animation_metadata_case(
        "coregraphics apng animation metadata",
        "/tests/data/inputs/formats/apng_8x8_rgb_loop2.png");
    if (result != 0) {
        return result;
    }
    result = run_coregraphics_start_frame_case(
        "coregraphics apng start-frame behavior",
        "/tests/data/inputs/formats/apng_8x8_rgb_loop2.png");
    if (result != 0) {
        return result;
    }
    result = run_coregraphics_loop_control_case(
        "coregraphics apng loop-control behavior",
        "/tests/data/inputs/formats/apng_8x8_rgb_loop2.png");
    if (result != 0) {
        return result;
    }

    if (coregraphics_runtime_supports_webp_animation()) {
        result = run_coregraphics_animation_metadata_case(
            "coregraphics webp animation metadata",
            WEBP_ANIMATED_IMAGE_PATH);
        if (result != 0) {
            return result;
        }
        result = run_coregraphics_start_frame_case(
            "coregraphics webp start-frame behavior",
            WEBP_ANIMATED_IMAGE_PATH);
        if (result != 0) {
            return result;
        }
        result = run_coregraphics_loop_control_case(
            "coregraphics webp loop-control behavior",
            WEBP_LOOP2_IMAGE_PATH);
        if (result != 0) {
            return result;
        }
    }

    return 0;
}
#endif

int
test_loader_0008_loader_coregraphics_pixelformat(int argc, char **argv)
{
    (void) argc;
    (void) argv;

#if HAVE_COREGRAPHICS
    return run_coregraphics_loader_test();
#else
    fprintf(stderr, "CoreGraphics loader unavailable\n");
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
