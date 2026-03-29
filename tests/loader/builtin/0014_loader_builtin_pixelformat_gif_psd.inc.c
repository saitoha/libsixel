static int
run_builtin_loader_gif_sequence_test(
    char const *label,
    char const *relative_path,
    int loop_control,
    int const *expected_sequence,
    int expected_count)
{
    SIXELSTATUS status;
    builtin_loader_probe_options_t options;
    gif_sequence_probe_context_t probe;
    int mismatch_expected_loop_no;
    int mismatch_expected_frame_no;
    int result;

    status = SIXEL_FALSE;
    mismatch_expected_loop_no = -1;
    mismatch_expected_frame_no = -1;
    result = 1;
    memset(&probe, 0, sizeof(probe));

    if (label == NULL ||
        relative_path == NULL ||
        expected_sequence == NULL ||
        expected_count <= 0) {
        return 1;
    }

    probe.callback_count = 0;
    probe.expected_count = expected_count;
    probe.mismatch_index = -1;
    probe.mismatch_loop_no = -1;
    probe.mismatch_frame_no = -1;
    probe.saw_multiframe = 0;
    probe.expected_sequence = expected_sequence;
    options.require_static = 0;
    options.use_palette = 1;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 1;
    options.loop_control = loop_control;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;
    result = run_builtin_loader_probe_case(label,
                                           relative_path,
                                           &options,
                                           capture_gif_sequence_probe,
                                           &probe,
                                           &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        if (probe.mismatch_index >= 0 &&
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
                    "%s: loader returned failure (%d)\n",
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
run_builtin_loader_gif_loop_disable_loop0_once_test(void)
{
    static int const expected_sequence[] = { 0, 0, 0, 1 };

    return run_builtin_loader_gif_sequence_test(
        "builtin gif loop=disable ignores loop0 and emits one pass",
        "/tests/data/inputs/formats/gif-anim-netscape-loop0.gif",
        SIXEL_LOOP_DISABLE,
        expected_sequence,
        2);
}

static int
run_builtin_loader_gif_loop_disable_loop1_once_test(void)
{
    static int const expected_sequence[] = { 0, 0, 0, 1 };

    return run_builtin_loader_gif_sequence_test(
        "builtin gif loop=disable ignores loop1 and emits one pass",
        "/tests/data/inputs/formats/gif-anim-netscape-loop1.gif",
        SIXEL_LOOP_DISABLE,
        expected_sequence,
        2);
}

static int
run_builtin_loader_gif_loop_disable_loop2_once_test(void)
{
    static int const expected_sequence[] = { 0, 0, 0, 1 };

    return run_builtin_loader_gif_sequence_test(
        "builtin gif loop=disable ignores loop2 and emits one pass",
        "/tests/data/inputs/formats/gif-anim-netscape-loop2.gif",
        SIXEL_LOOP_DISABLE,
        expected_sequence,
        2);
}

static int
run_builtin_loader_gif_loop_auto_loop1_once_test(void)
{
    static int const expected_sequence[] = { 0, 0, 0, 1 };

    return run_builtin_loader_gif_sequence_test(
        "builtin gif loop=auto respects loop1 as one pass",
        "/tests/data/inputs/formats/gif-anim-netscape-loop1.gif",
        SIXEL_LOOP_AUTO,
        expected_sequence,
        2);
}

static int
run_builtin_loader_gif_loop_auto_loop2_twice_test(void)
{
    static int const expected_sequence[] = {
        0, 0,
        0, 1,
        1, 0,
        1, 1
    };

    return run_builtin_loader_gif_sequence_test(
        "builtin gif loop=auto respects loop2 as two passes",
        "/tests/data/inputs/formats/gif-anim-netscape-loop2.gif",
        SIXEL_LOOP_AUTO,
        expected_sequence,
        4);
}

static int
run_builtin_loader_gif_unbounded_loop_probe_test(
    char const *label,
    char const *relative_path,
    int loop_control,
    int required_loop_no)
{
    SIXELSTATUS status;
    builtin_loader_probe_options_t options;
    gif_loop_probe_context_t probe;
    int result;

    status = SIXEL_FALSE;
    result = 1;
    memset(&probe, 0, sizeof(probe));

    if (label == NULL || relative_path == NULL || required_loop_no < 0) {
        return 1;
    }

    probe.callback_count = 0;
    /* Keep probe bounded while still allowing several full animation loops. */
    probe.max_callbacks = 64;
    probe.required_loop_no = required_loop_no;
    probe.highest_loop_no = -1;
    probe.highest_frame_no = -1;
    probe.saw_multiframe = 0;
    probe.reached_required_loop = 0;
    options.require_static = 0;
    options.use_palette = 1;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 1;
    options.loop_control = loop_control;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;
    result = run_builtin_loader_probe_case(label,
                                           relative_path,
                                           &options,
                                           capture_gif_loop_probe_until_target,
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
                "%s: loop threshold was not reached "
                "(required=%d highest=%d callbacks=%d)\n",
                label,
                required_loop_no,
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
run_builtin_loader_gif_loop_auto_loop0_unbounded_test(void)
{
    return run_builtin_loader_gif_unbounded_loop_probe_test(
        "builtin gif loop=auto loop0 remains unbounded",
        "/tests/data/inputs/formats/gif-anim-netscape-loop0.gif",
        SIXEL_LOOP_AUTO,
        2);
}

static int
run_builtin_loader_gif_loop_force_loop0_unbounded_test(void)
{
    return run_builtin_loader_gif_unbounded_loop_probe_test(
        "builtin gif loop=force loop0 remains unbounded",
        "/tests/data/inputs/formats/gif-anim-netscape-loop0.gif",
        SIXEL_LOOP_FORCE,
        2);
}

static int
run_builtin_loader_gif_loop_force_loop1_unbounded_test(void)
{
    return run_builtin_loader_gif_unbounded_loop_probe_test(
        "builtin gif loop=force ignores loop1 and stays unbounded",
        "/tests/data/inputs/formats/gif-anim-netscape-loop1.gif",
        SIXEL_LOOP_FORCE,
        2);
}

static int
run_builtin_loader_gif_loop_force_loop2_unbounded_test(void)
{
    return run_builtin_loader_gif_unbounded_loop_probe_test(
        "builtin gif loop=force ignores loop2 and stays unbounded",
        "/tests/data/inputs/formats/gif-anim-netscape-loop2.gif",
        SIXEL_LOOP_FORCE,
        3);
}

static int
run_builtin_loader_hdr_exposure_overflow_none_numeric_test(void)
{
    return run_builtin_loader_hdr_static_numeric_case(
        &hdr_numeric_static_cases[
            HDR_NUMERIC_STATIC_CASE_EXPOSURE_OVERFLOW_NONE]);
}

static int
run_builtin_loader_hdr_exposure_overflow_reinhard_numeric_test(void)
{
    return run_builtin_loader_hdr_static_numeric_case(
        &hdr_numeric_static_cases[
            HDR_NUMERIC_STATIC_CASE_EXPOSURE_OVERFLOW_REINHARD]);
}

static int
run_builtin_loader_hdr_case_with_cms(char const *label,
                                     int expected_pixelformat,
                                     int expected_colorspace,
                                     int cms_engine)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_chunk_t *chunk;
    sixel_loader_component_t *component;
    loader_probe_context_t context;
    loader_probe_callback_state_t callback_state;
    char const *source_root;
    char image_path[PATH_MAX];
    int cancel_flag;
    int result;
    int require_static;
    int use_palette;
    int reqcolors;

    status = SIXEL_FALSE;
    allocator = NULL;
    chunk = NULL;
    component = NULL;
    source_root = NULL;
    cancel_flag = 0;
    result = 1;
    require_static = 1;
    use_palette = 0;
    reqcolors = 256;

    source_root = resolve_source_root_for_pixelformat_test();

    if (build_image_path(source_root,
                         "/tests/data/inputs/formats/stbi_minimal.hdr",
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

    status = sixel_chunk_new(&chunk, image_path, 0, &cancel_flag, allocator);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: failed to read sample\n", label);
        goto cleanup;
    }

    status = new_builtin_component_for_pixelformat_test(allocator, &component);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: component init failed (%d)\n", label, (int)status);
        goto cleanup;
    }

    context.callback_count = 0;
    context.pixelformat = 0;
    context.colorspace = 0;
    context.width = 0;
    context.height = 0;
    context.transparent = FRAME_METADATA_ANY;
    context.multiframe = FRAME_METADATA_ANY;
    callback_state.loader = NULL;
    callback_state.fn = capture_frame;
    callback_state.context = &context;

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
                                           SIXEL_LOADER_COMPONENT_OPTION_CMS_ENGINE,
                                           &cms_engine);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = sixel_loader_component_load(component,
                                         chunk,
                                         capture_frame_trampoline,
                                         &callback_state);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "%s: loader reported failure (%d)\n",
                label,
                (int)status);
        goto cleanup;
    }

    if (context.callback_count != 1) {
        fprintf(stderr, "%s: callback count mismatch\n", label);
        goto cleanup;
    }
    if (context.pixelformat != expected_pixelformat) {
        fprintf(stderr,
                "%s: reported pixelformat %d\n",
                label,
                context.pixelformat);
        goto cleanup;
    }
    if (context.colorspace != expected_colorspace) {
        fprintf(stderr,
                "%s: reported colorspace %d\n",
                label,
                context.colorspace);
        goto cleanup;
    }
    if (context.width <= 0 || context.height <= 0) {
        fprintf(stderr,
                "%s: invalid geometry %dx%d\n",
                label,
                context.width,
                context.height);
        goto cleanup;
    }

    result = 0;

cleanup:
    sixel_loader_component_unref(component);
    sixel_chunk_destroy(chunk);
    sixel_allocator_unref(allocator);
    return result;
}

static int
run_builtin_loader_psd_validate_defensive_test(void)
{
#if defined(_WIN32)
    /*
     * this test calls non-exported builtin PSD validation symbols directly.
     * windows CI links test_runner against the DLL import library, so keep the
     * defensive-unit branch on non-Windows targets where internal symbols are
     * link-visible.
     */
    return 0;
#else
    sixel_chunk_t chunk;
    sixel_builtin_psd_info_t info;
    unsigned char buffer[64];
    int decode_mode;
    int skip_icc_conversion;
    int colorspace;
    char message[128];
    int status;
    int use_32bit_overflow_case;

    memset(&chunk, 0, sizeof(chunk));
    memset(&info, 0, sizeof(info));
    memset(buffer, 0, sizeof(buffer));
    memset(message, 0, sizeof(message));
    decode_mode = SIXEL_BUILTIN_PSD_DECODE_MODE_NONE;
    skip_icc_conversion = 0;
    colorspace = SIXEL_COLORSPACE_GAMMA;
    use_32bit_overflow_case = 0;
    /*
     * SIZE_MAX can be toolchain-dependent in split include units, so keep the
     * branch selection tied to the actual size_t width of this build target.
     */
    use_32bit_overflow_case = (sizeof(size_t) <= 4u);

    status = sixel_builtin_validate_psd_info(NULL,
                                             NULL,
                                             &decode_mode,
                                             &skip_icc_conversion,
                                             &colorspace,
                                             message,
                                             sizeof(message));
    if (status != SIXEL_BUILTIN_PSD_VALIDATE_MALFORMED ||
        strcmp(message, "builtin PSD: malformed header/metadata") != 0) {
        fprintf(stderr,
                "builtin psd validate defensive: malformed header path mismatch "
                "(status=%d message=%s)\n",
                status,
                message);
        return 1;
    }

    chunk.buffer = buffer;
    chunk.size = sizeof(buffer);
    info.version = 1u;
    info.channels = 3u;
    info.width = 1u;
    info.height = 1u;
    info.depth = 8u;
    info.color_mode = 3u;
    info.compression = 0u;
    info.image_data_offset = chunk.size + 1u;
    message[0] = '\0';
    status = sixel_builtin_validate_psd_info(&chunk,
                                             &info,
                                             &decode_mode,
                                             &skip_icc_conversion,
                                             &colorspace,
                                             message,
                                             sizeof(message));
    if (status != SIXEL_BUILTIN_PSD_VALIDATE_MALFORMED ||
        strcmp(message, "builtin PSD: malformed image data offset") != 0) {
        fprintf(stderr,
                "builtin psd validate defensive: image data offset mismatch "
                "(status=%d message=%s)\n",
                status,
                message);
        return 1;
    }

    if (use_32bit_overflow_case) {
        info.version = 1u;
        info.channels = 3u;
        info.width = 300000u;
        info.height = 300000u;
        info.depth = 32u;
        info.color_mode = 3u;
        info.compression = 0u;
        info.image_data_offset = 2u;
        message[0] = '\0';
        status = sixel_builtin_validate_psd_info(&chunk,
                                                 &info,
                                                 &decode_mode,
                                                 &skip_icc_conversion,
                                                 &colorspace,
                                                 message,
                                                 sizeof(message));
        if (status != SIXEL_BUILTIN_PSD_VALIDATE_MALFORMED ||
            strcmp(message, "builtin PSD: malformed dimensions/depth overflow")
                != 0) {
            fprintf(stderr,
                    "builtin psd validate defensive: overflow path mismatch "
                    "(status=%d message=%s)\n",
                    status,
                    message);
            return 1;
        }
    }

    return 0;
#endif
}
