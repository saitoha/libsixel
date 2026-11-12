/* SPDX-License-Identifier: MIT */
/* Composition root for the fixed RGB888 representation. Admission is before
 * output; unsupported precision, coverage and wire modes use the legacy
 * adapter. Once output starts, failures never retry a different backend. */
static SIXELSTATUS
sixel_session_write(char const *bytes, size_t size, void *context)
{
    sixel_writer_t *writer;

    writer = context;
    if (size > INT_MAX) return SIXEL_BAD_INTEGER_OVERFLOW;
    return writer->vtbl->write(writer, bytes, (int)size);
}

static SIXELSTATUS
sixel_session_rows(sixel_indexed_frame_t *image, int rows, void *context)
{
    sixel_encoder_core_t *core;

    (void)image;
    core = context;
    return core->vtbl->consume(core, rows);
}

/* The physical stream filter drives its producer while complete published
 * bands are consumed. Nesting here is scheduling, not a frame barrier: row
 * notifications call the same body consumer before PaletteApply returns. */

typedef struct sixel_indexed_subgraph {
    sixel_filter_t apply_filter;
    sixel_filter_t stream_filter;
    sixel_filter_t commit_filter;
    sixel_palette_apply_interface_t *apply;
    sixel_encoder_core_t *core;
    sixel_writer_t *writer;
    sixel_writer_image_header_t header;
    sixel_palette_apply_run_request_t *run;
    sixel_encode_session_t *session;
    sixel_retained_update_t *update;
    sixel_output_deferred_t *prepared;
    int opened;
    int consumed;
    int applied;
    SIXELSTATUS apply_status;
} sixel_indexed_subgraph_t;

static SIXELSTATUS
sixel_indexed_apply_filter(sixel_filter_t *filter,
    sixel_allocator_t *allocator, sixel_timeline_logger_t *logger)
{
    sixel_indexed_subgraph_t *graph;
    SIXELSTATUS status;

    (void)allocator;
    graph = filter->userdata;
    graph->run->source = sixel_frame_as_interface(*filter->input.slot);
    graph->run->baseline = *filter->input.retained;
    graph->run->result = *filter->output.indexed;
    graph->applied = 1;
    sixel_timeline_logger_logf(logger, "controller", "dither",
        "apply_start", -1);
    status = graph->apply->vtbl->run(graph->apply, graph->run);
    graph->apply_status = status;
    sixel_timeline_logger_logf(logger, "controller", "dither",
        "apply_finish", -1);
    return status;
}

static SIXELSTATUS
sixel_indexed_stream_filter(sixel_filter_t *filter,
    sixel_allocator_t *allocator, sixel_timeline_logger_t *logger)
{
    sixel_indexed_subgraph_t *graph;
    sixel_indexed_frame_t *image;
    SIXELSTATUS status;
    int body_started;

    graph = filter->userdata;
    image = *filter->input.indexed;
    body_started = 0;
    graph->opened = 1;
    if (graph->prepared != NULL) graph->prepared->output_started = 1;
    if (graph->session != NULL) graph->session->partial_output = 1;
    status = graph->writer->vtbl->begin_image(graph->writer, &graph->header,
        &image->identity);
    if (status != SIXEL_OK) return status;
    if (graph->run->rows_ready != NULL) {
        sixel_timeline_logger_logf(logger, "controller", "encode",
            "body_start", -1);
        body_started = 1;
    }
    if (!graph->applied) {
        status = sixel_filter_run(&graph->apply_filter, allocator, logger);
        if (status != SIXEL_OK) goto end;
    }
    if (graph->run->rows_ready == NULL) {
        sixel_timeline_logger_logf(logger, "controller", "encode",
            "body_start", -1);
        body_started = 1;
        status = graph->core->vtbl->consume(graph->core,
            image->description.height);
        if (status != SIXEL_OK) goto end;
    }
    if (graph->session != NULL) {
        sixel_timeline_logger_logf(logger, "controller", "retained_stage",
            "start", -1);
        status = graph->session->plane->vtbl->stage(graph->session->plane,
            graph->update);
        sixel_timeline_logger_logf(logger, "controller", "retained_stage",
            "finish", -1);
        if (status != SIXEL_OK) goto end;
    }
    status = graph->core->vtbl->finish(graph->core);
    if (status != SIXEL_OK) goto end;
    graph->consumed = 0;
    status = graph->writer->vtbl->end_image(graph->writer,
        filter->output.receipt);
    if (status == SIXEL_OK) graph->opened = 0;
end:
    if (body_started) {
        sixel_timeline_logger_logf(logger, "controller", "encode",
            "body_finish", -1);
    }
    return status;
}

static SIXELSTATUS
sixel_indexed_commit_filter(sixel_filter_t *filter,
    sixel_allocator_t *allocator, sixel_timeline_logger_t *logger)
{
    sixel_indexed_subgraph_t *graph;
    SIXELSTATUS status;

    (void)allocator;
    graph = filter->userdata;
    /* The completed stream drained its producer and body consumers. Release
     * the session read lease before exclusive, allocation-free publication. */
    sixel_retained_read_unref(*filter->input.retained);
    *filter->input.retained = NULL;
    sixel_timeline_logger_logf(logger, "controller", "retained_commit",
        "start", -1);
    status = graph->session->plane->vtbl->commit(graph->session->plane,
        graph->update, *filter->input.receipt, &graph->session->stats);
    sixel_timeline_logger_logf(logger, "controller", "retained_commit",
        "finish", -1);
    if (status == SIXEL_OK) {
        sixel_timeline_logger_logf(logger, "controller", "retained_commit",
            "workers", graph->session->stats.worker_threads);
        graph->session->committed = 1;
        graph->session->partial_output = 0;
    }
    return status;
}

/* Freeze palette, geometry, source wrapper and application capability
 * before acquiring retained state. This helper also serves direct encoding. */
static SIXELSTATUS
sixel_indexed_prepare(sixel_legacy_encode_request_t const *request,
    sixel_output_deferred_t *prepared)
{
    sixel_dither_t *dither;
    sixel_output_t *output;
    sixel_encode_session_t *session;
    sixel_palette_apply_interface_t *apply;
    sixel_frame_t *source;
    sixel_wire_palette_t *wire;
    sixel_palette_entries_view_t entries;
    sixel_wire_color_definition_t definitions[SIXEL_PALETTE_MAX];
    unsigned char colors[SIXEL_PALETTE_MAX * 3];
    sixel_wire_palette_request_t wire_request;
    sixel_indexed_frame_description_t description;
    sixel_dither_policy_prepare_request_t configuration;
    sixel_palette_apply_prepare_request_t prepare;
    sixel_dither_encode_allocation_t allocation;
    double rgb[3];
    void *object;
    size_t pixels, i;
    int channel, threads;
    SIXELSTATUS status;

    dither = request->dither;
    output = request->output;
    session = output->session;
    threads = sixel_threads_resolve();
    source = NULL;
    wire = NULL;
    apply = NULL;
    memset(&prepare, 0, sizeof(prepare));
    status = sixel_dither_prepare_indexed_components(dither, threads > 1, 0);
    if (status != SIXEL_OK) goto end;
    pixels = (size_t)request->width * (size_t)request->height;
    memset(&wire_request, 0, sizeof(wire_request));
    memset(&description, 0, sizeof(description));
    memset(&prepare, 0, sizeof(prepare));
    status = dither->palette->vtbl->get_entries(dither->palette, &entries);
    if (SIXEL_FAILED(status)) goto end;
    if (entries.entry_count == 0 || entries.entry_count > SIXEL_PALETTE_MAX) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    memcpy(colors, entries.entries, entries.entry_count * 3);
    if (dither->palette_snap_exact) {
        sixel_palette_snap_output(colors, NULL, entries.entry_count);
    }
    for (i = 0; i < entries.entry_count; ++i) {
        definitions[i].register_index = (unsigned int)i;
        definitions[i].type = output->palette_type == SIXEL_PALETTETYPE_HLS
            ? 1 : 2;
        if (definitions[i].type == 1) {
            for (channel = 0; channel < 3; ++channel) {
                rgb[channel] = colors[i * 3 + channel] / 255.0;
            }
            sixel_color_rgb_to_hls(definitions[i].fields, rgb);
        } else {
            for (channel = 0; channel < 3; ++channel) {
                definitions[i].fields[channel] =
                    (colors[i * 3 + channel] * 100 + 127) / 255;
            }
        }
    }
    wire_request.definitions = definitions;
    wire_request.count = entries.entry_count;
    wire_request.color_definition = SIXEL_PALETTE_OUTPUT_LEGACY;
    wire_request.colorspace = SIXEL_COLORSPACE_GAMMA;
    status = sixel_wire_palette_new(&wire, &wire_request, dither->allocator);
    if (SIXEL_FAILED(status)) goto end;
    description.width = request->width;
    description.height = request->height;
    description.encoded_x = output->transparent_offset_left;
    description.encoded_y = output->transparent_offset_top;
    status = sixel_output_compute_transparent_extent(output, request->width,
        request->height, &description.encoded_width,
        &description.encoded_height);
    if (SIXEL_FAILED(status)) goto end;
    description.origin_x = session != NULL ? session->origin_x : 0;
    description.origin_y = session != NULL ? session->origin_y : 0;
    description.plane_width = session != NULL ? session->plane_width
        : request->width;
    description.plane_height = session != NULL ? session->plane_height
        : request->height;
    description.palette = wire;
    description.keycolor = dither->keycolor;
    description.keycolor_reserved = dither->keycolor_reserved;
    description.transparent_background = dither->keycolor >= 0 ||
        output->require_keep_header ||
        sixel_output_has_transparent_offset(output);
    status = sixel_frame_new(&source, dither->allocator);
    if (SIXEL_FAILED(status)) goto end;
    status = sixel_frame_init_borrowed(source, request->pixels,
        request->width, request->height, SIXEL_PIXELFORMAT_RGB888, NULL, -1);
    if (SIXEL_FAILED(status)) goto end;
    status = dither->gpu_dither != NULL
        ? dither->gpu_dither->vtbl->get_configuration(dither->gpu_dither,
            &configuration)
        : dither->dither_policy->vtbl->get_configuration(
            dither->dither_policy, &configuration);
    if (SIXEL_FAILED(status)) goto end;
#if SIXEL_ENABLE_THREADS
    sixel_parallel_dither_configure(request->height, threads, pixels,
        dither, &allocation);
#else
    (void)pixels;
    (void)threads;
    memset(&allocation, 0, sizeof(allocation));
    allocation.mode = SIXEL_DITHER_ENCODE_SERIAL;
    allocation.dither_threads = allocation.encode_threads = 1;
#endif
    prepare.working_palette = dither->palette;
    prepare.dither = dither->dither_policy;
    prepare.gpu_dither = dither->gpu_dither;
    prepare.output = description;
    prepare.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    prepare.colorspace = SIXEL_COLORSPACE_GAMMA;
    prepare.scan = configuration.method_for_scan;
    prepare.worker_threads = allocation.dither_threads > 0
        ? allocation.dither_threads : 1;
    prepare.band_height = allocation.band_height;
    prepare.band_overlap = allocation.overlap;
    prepare.gpu_policy = dither->gpu_policy;
    prepare.gpu_auto_threshold = dither->gpu_palette_threshold;
    status = sixel_dither_lookup_preparers(dither, &prepare);
    if (status != SIXEL_OK) goto end;
    /* A direct-apply-only backend (Vulkan) has no runtime device. Admit it
     * with the legacy apply predicate so R and the synchronous path agree. */
    if (prepare.device == NULL && prepare.gpu_dither == NULL &&
            sixel_gpu_palette_direct_apply_serves(dither->lut_policy,
                dither->method_for_diffuse) &&
            sixel_gpu_palette_policy_claims_apply_stage(dither->gpu_policy,
                dither->gpu_palette_threshold, dither->lut_policy,
                dither->method_for_diffuse, prepare.scan, pixels)) {
        status = sixel_dither_get_lookup_configuration(dither,
            prepare.pixelformat, &prepare.gpu_direct_lookup);
        if (status != SIXEL_OK) goto end;
        if (!sixel_gpu_palette_direct_lookup_serves(&prepare.gpu_direct_lookup,
                dither->method_for_diffuse))
            memset(&prepare.gpu_direct_lookup, 0,
                sizeof(prepare.gpu_direct_lookup));
    }
    object = NULL;
    status = sixel_palette_apply_factory_new(dither->allocator, &object);
    if (SIXEL_FAILED(status)) goto end;
    apply = object;
    status = apply->vtbl->prepare(apply, &prepare);
    if (status == SIXEL_FEATURE_ERROR && prepare.dither != NULL) {
        /* Unsupported plans keep the legacy admission/framing behavior. */
        status = SIXEL_FALSE;
    }
    if (status != SIXEL_OK) goto end;
    prepared->apply = apply;
    prepared->source = source;
    prepared->description = description;
    apply = NULL;
    source = NULL;
    wire = NULL;
end:
    sixel_wire_palette_unref(wire);
    sixel_frame_unref(source);
    if (apply != NULL) apply->vtbl->unref(apply);
    if (prepare.cpu_lookup != NULL)
        prepare.cpu_lookup->vtbl->unref(prepare.cpu_lookup);
    if (prepare.gpu_lookup != NULL)
        prepare.gpu_lookup->vtbl->unref(prepare.gpu_lookup);
    if (prepare.device != NULL) prepare.device->vtbl->unref(prepare.device);
    return status;
}

void
sixel_output_dispose_deferred(sixel_output_deferred_t *prepared)
{
    if (prepared->solution != NULL)
        prepared->solution->vtbl->unref(prepared->solution);
    if (prepared->capability != NULL)
        prepared->capability->vtbl->unref(prepared->capability);
    if (prepared->apply != NULL)
        prepared->apply->vtbl->unref(prepared->apply);
    sixel_frame_unref(prepared->source);
    sixel_wire_palette_unref(prepared->description.palette);
    memset(prepared, 0, sizeof(*prepared));
}

SIXELSTATUS
sixel_output_prepare_deferred(sixel_legacy_encode_request_t const *request,
    sixel_output_deferred_t *prepared)
{
    SIXELSTATUS status;

    status = sixel_indexed_prepare(request, prepared);
    if (status == SIXEL_OK)
        status = prepared->apply->vtbl->get_deferred(prepared->apply,
            &prepared->capability);
    if (status != SIXEL_OK) sixel_output_dispose_deferred(prepared);
    return status;
}

SIXELSTATUS
sixel_output_solve_deferred(sixel_output_deferred_t *prepared,
    sixel_timeline_logger_t *logger, sixel_cancel_function cancel,
    void *cancel_context)
{
    sixel_palette_apply_solve_request_t solve;
    SIXELSTATUS status;
    double start;

    memset(&solve, 0, sizeof(solve));
    solve.source = sixel_frame_as_interface(prepared->source);
    solve.logger = logger;
    solve.cancel = cancel;
    solve.cancel_context = cancel_context;
    prepared->solve_started = sixel_timer_now();
    start = sixel_timer_monotonic();
    status = prepared->capability->vtbl->solve(prepared->capability, &solve,
        &prepared->solution);
    prepared->solve_completed = sixel_timer_monotonic();
    if (status == SIXEL_OK) {
        prepared->stages.valid |= SIXEL_PLAYBACK_STAGE_SOLVE |
            SIXEL_PLAYBACK_STAGE_DEFERRED;
        prepared->stages.solve_seconds = prepared->solve_completed -
            start;
    }
    return status;
}

void
sixel_output_bind_deferred(sixel_output_t *output,
    sixel_output_deferred_t *prepared)
{
    output->deferred = prepared;
}

int
sixel_output_is_resolve_only(sixel_output_t const *output)
{
    return output != NULL && output->deferred != NULL &&
        output->deferred->capture != NULL;
}

/* E owns only this complete image and its private packet writer. Passing a
 * null identity avoids allocating an acceptance receipt that R never uses. */
SIXELSTATUS
sixel_output_encode_resolved(sixel_output_t *output,
    sixel_output_resolved_t const *resolved)
{
    sixel_encoder_core_prepare_request_t body;
    SIXELSTATUS status;
    int opened;

    if (output == NULL || resolved == NULL || resolved->image == NULL ||
            sixel_indexed_frame_get_state(resolved->image) !=
                SIXEL_INDEXED_COMPLETE) return SIXEL_BAD_ARGUMENT;
    opened = 0;
    if (output->core == NULL) {
        status = sixel_encoder_core_create_interface_from_factory(
            output->allocator, &output->core);
        if (status != SIXEL_OK) return status;
    }
    memset(&body, 0, sizeof(body));
    body.image = resolved->image;
    body.options = resolved->options;
    body.threads = resolved->threads;
    body.worker_capacity = body.threads;
    body.queue_bands = body.threads * 3;
    body.sink.write = sixel_session_write;
    body.sink.context = output->writer;
    status = output->core->vtbl->prepare(output->core, &body);
    if (status != SIXEL_OK) return status;
    status = output->writer->vtbl->begin_image(output->writer,
        &resolved->header, NULL);
    if (status != SIXEL_OK) goto end;
    opened = 1;
    status = output->core->vtbl->consume(output->core,
        resolved->image->description.height);
    if (status == SIXEL_OK)
        status = output->core->vtbl->finish(output->core);
    if (status == SIXEL_OK) {
        status = output->writer->vtbl->end_image(output->writer, NULL);
        opened = 0;
    }
end:
    if (status != SIXEL_OK) (void)output->core->vtbl->abort(output->core);
    if (opened) (void)output->writer->vtbl->abort_image(output->writer);
    return status;
}

static SIXELSTATUS
sixel_encode_indexed_session(sixel_legacy_encode_request_t const *request)
{
    sixel_dither_t *dither;
    sixel_timeline_logger_t *logger;
    sixel_timeline_logger_t *saved_logger;
    sixel_output_t *output;
    sixel_encode_session_t *session;
    sixel_palette_apply_interface_t *apply;
    sixel_gpu_device_interface_t *retained_device;
    sixel_frame_t *source;
    sixel_indexed_frame_t *image;
    sixel_retained_read_t *read;
    sixel_retained_update_t *update;
    sixel_stream_receipt_t *receipt;
    sixel_indexed_frame_description_t description;
    sixel_palette_apply_run_request_t run;
    sixel_encoder_core_prepare_request_t body;
    sixel_writer_image_header_t header;
    sixel_retained_read_view_t baseline;
    sixel_retained_update_request_t update_request;
    sixel_dither_execution_t execution;
    sixel_dither_encode_allocation_t allocation;
    sixel_indexed_subgraph_t graph;
    size_t pixels;
    int threads;
    sixel_output_deferred_t local, *prepared;
    sixel_palette_apply_resolve_request_t resolve;
    double resolve_started, emit_started, history_started;
    SIXELSTATUS status;

    dither = request->dither;
    output = request->output;
    session = output->session;
    /* No change to the legacy mono shortcut, explicit precision or output
     * interpretation is allowed merely to qualify for this physical plan. */
    if (output->indexed_disabled ||
            !sixel_encode_delta_is_legacy(dither, output) ||
            output->source_colorspace != SIXEL_COLORSPACE_GAMMA ||
            (dither->keycolor >= 0 &&
             output->transparent_policy != SIXEL_ALPHA_POLICY_KEEP &&
             output->transparent_policy != SIXEL_ALPHA_POLICY_COMPOSITE) ||
            ((dither->pipeline_accumulation_result_enabled ||
              dither->pipeline_6delta_enabled) && session == NULL)) {
        return SIXEL_FALSE;
    }
    threads = sixel_threads_resolve();

    memset(&local, 0, sizeof(local));
    emit_started = 0;
    source = NULL;
    image = NULL;
    read = NULL;
    update = NULL;
    receipt = NULL;
    apply = NULL;
    memset(&graph, 0, sizeof(graph));
    saved_logger = dither->pipeline_logger;
    logger = saved_logger;
    if (logger == NULL) {
        (void)sixel_timeline_logger_prepare_env(dither->allocator, &logger);
    }
    dither->pipeline_logger = logger;
    memset(&local, 0, sizeof(local));
    prepared = output->deferred;
    if (prepared == NULL || prepared->solution == NULL) {
        status = sixel_indexed_prepare(request, &local);
        if (status != SIXEL_OK) goto end;
        if (prepared != NULL) local.capture = prepared->capture;
        prepared = &local;
    }
    apply = prepared->apply;
    source = prepared->source;
    description = prepared->description;
    /* Recheck the consumer's geometry rather than silently relabeling a
     * speculative result after a setting or origin change. */
    if (description.width != request->width ||
            description.height != request->height ||
            description.keycolor != dither->keycolor ||
            description.keycolor_reserved != dither->keycolor_reserved ||
            (session != NULL &&
             (description.origin_x != session->origin_x ||
              description.origin_y != session->origin_y ||
              description.plane_width != session->plane_width ||
              description.plane_height != session->plane_height))) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    status = sixel_indexed_frame_new(&image, &description, dither->allocator);
    if (status != SIXEL_OK) goto end;
    pixels = (size_t)request->width * request->height;
#if SIXEL_ENABLE_THREADS
    sixel_parallel_dither_configure(request->height, threads, pixels,
        dither, &allocation);
#else
    (void)pixels;
    (void)threads;
    memset(&allocation, 0, sizeof(allocation));
    allocation.mode = SIXEL_DITHER_ENCODE_SERIAL;
    allocation.dither_threads = allocation.encode_threads = 1;
#endif
    memset(&run, 0, sizeof(run));
    memset(&body, 0, sizeof(body));
    memset(&header, 0, sizeof(header));
    memset(&update_request, 0, sizeof(update_request));
    if (session != NULL) {
        status = apply->vtbl->get_device(apply, &retained_device);
        if (status != SIXEL_OK) goto end;
        status = session->plane->vtbl->set_device(session->plane,
            retained_device);
        if (status != SIXEL_OK && status != SIXEL_FEATURE_ERROR) goto end;
        status = session->plane->vtbl->acquire_read(session->plane, &read);
        if (SIXEL_FAILED(status)) goto end;
        status = sixel_retained_read_get_view(read, &baseline);
        if (SIXEL_FAILED(status)) goto end;
        if (baseline.sequence == UINT64_MAX) {
            status = SIXEL_BAD_INTEGER_OVERFLOW;
            goto end;
        }
        update_request.baseline = read;
        update_request.image = image;
        update_request.sequence = baseline.sequence + 1;
        update_request.record_paint = session->record_paint;
        /* Direct commit reuses the drained encode budget. Independent R
         * uses this per-frame cap without an E completion dependency. GPU
         * history reports no CPU participants. */
        update_request.worker_threads = allocation.encode_threads;
        status = prepared->capture != NULL
            ? session->plane->vtbl->begin_resolved_update(session->plane,
                &update_request, &update)
            : session->plane->vtbl->begin_update(session->plane,
                &update_request, &update);
        if (SIXEL_FAILED(status)) goto end;
    }
    if (prepared->capture == NULL && output->core == NULL) {
        status = sixel_encoder_core_create_interface_from_factory(
            output->allocator, &output->core);
        if (SIXEL_FAILED(status)) goto end;
    }
    body.image = image;
    body.options.has_gri_arg_limit = output->has_gri_arg_limit;
    body.options.encode_policy = output->encode_policy;
    body.options.emit_palette = 1;
    body.threads = allocation.encode_threads;
    body.queue_bands = body.threads * 3;
    body.worker_capacity = body.threads;
    if (allocation.mode == SIXEL_DITHER_ENCODE_PIPELINE) {
        body.worker_capacity += allocation.dither_threads;
    }
    body.logger = logger;
    body.sink.write = sixel_session_write;
    body.sink.context = output->writer;
    if (prepared->capture == NULL) {
        status = output->core->vtbl->prepare(output->core, &body);
        if (SIXEL_FAILED(status)) goto end;
        graph.consumed = 1;
    }
    header.parameter_count = description.transparent_background ? 2 : 0;
    header.parameter1 = description.transparent_background;
    header.width = description.encoded_width;
    header.height = description.encoded_height;
    header.use_raster_attributes = 1;
    sixel_dither_policy_capture_execution(dither, &execution);
    if (session != NULL) {
        run.cancel = session->cancel;
        run.cancel_context = session->cancel_context;
    }
    run.logger = logger;
    run.source = sixel_frame_as_interface(source);
    run.result = image;
    run.baseline = read;
    run.delta.enabled = dither->pipeline_6delta_enabled;
    run.delta.error_mode = dither->pipeline_6delta_error_mode;
    run.delta.threshold = dither->pipeline_6delta_threshold;
    run.delta.metric = dither->pipeline_6delta_metric;
    run.delta.compare = dither->pipeline_6delta_compare;
    run.delta.selection = execution.selection;
    if (prepared->capture == NULL && prepared->solution == NULL &&
            allocation.mode == SIXEL_DITHER_ENCODE_PIPELINE) {
        run.rows_ready = sixel_session_rows;
        run.rows_context = output->core;
    }
    dither->pipeline_last_mode = allocation.mode;
    dither->pipeline_last_band_height = allocation.band_height;
    dither->pipeline_last_band_overlap = allocation.overlap;
    dither->pipeline_last_dither_threads = allocation.dither_threads;
    dither->pipeline_last_encode_threads = allocation.encode_threads;
    if (prepared->solution != NULL) {
        memset(&resolve, 0, sizeof(resolve));
        resolve.baseline = read;
        resolve.result = image;
        resolve.delta = run.delta;
        resolve.logger = logger;
        resolve.cancel = run.cancel;
        resolve.cancel_context = run.cancel_context;
        resolve_started = sixel_timer_monotonic();
        prepared->stages.ready_wait_seconds = resolve_started -
            prepared->solve_completed;
        prepared->stages.valid |= SIXEL_PLAYBACK_STAGE_READY_WAIT;
        status = prepared->solution->vtbl->resolve(prepared->solution,
            &resolve);
        if (status != SIXEL_OK) goto end;
        prepared->stages.resolve_seconds =
            sixel_timer_monotonic() - resolve_started;
        prepared->stages.valid |= SIXEL_PLAYBACK_STAGE_RESOLVE;
        graph.applied = 1;
        graph.apply_status = SIXEL_OK;
        /* Legacy two-worker callers can observe resolution here. Independent
         * playback releases solution ownership after history publication. */
        if (prepared->resolved != NULL)
            prepared->resolved(prepared->resolved_context);
        emit_started = sixel_timer_monotonic();
    }
    if (prepared->capture != NULL) {
        /* Resolve owns the only history edge. Reserve the destination before
         * entering this call; publication cannot be followed by queue loss. */
        if (!graph.applied) {
            status = apply->vtbl->run(apply, &run);
            if (status != SIXEL_OK) goto end;
        }
        history_started = sixel_timer_monotonic();
        if (session != NULL) {
            status = session->plane->vtbl->stage(session->plane, update);
            if (status != SIXEL_OK) goto end;
            sixel_retained_read_unref(read);
            read = NULL;
            status = session->plane->vtbl->advance_resolved(session->plane,
                update, &session->stats);
            if (status != SIXEL_OK) goto end;
            session->committed = 1;
            session->update = update;
            update = NULL;
        }
        prepared->history_seconds = sixel_timer_monotonic() - history_started;
        if (output->deferred != NULL)
            output->deferred->history_seconds = prepared->history_seconds;
        prepared->capture->image = image;
        prepared->capture->options = body.options;
        prepared->capture->header = header;
        prepared->capture->threads = body.threads;
        image = NULL;
        emit_started = 0;
        ++dither->apply_count;
        ++dither->apply_success_count;
        dither->last_apply_status = SIXEL_OK;
        ++output->indexed_count;
        goto end;
    }
    graph.prepared = prepared;
    graph.apply = apply;
    graph.core = output->core;
    graph.writer = output->writer;
    graph.header = header;
    graph.run = &run;
    graph.session = session;
    graph.update = update;
    (void)sixel_filter_init(&graph.apply_filter, "palette-apply",
        SIXEL_FILTER_KIND_PALETTE_APPLY, sixel_indexed_apply_filter,
        NULL, &graph);
    graph.apply_filter.input.slot = &source;
    graph.apply_filter.input.retained = &read;
    graph.apply_filter.output.indexed = &image;
    (void)sixel_filter_init(&graph.stream_filter, "indexed-sixel-stream",
        SIXEL_FILTER_KIND_SIXEL_STREAM, sixel_indexed_stream_filter,
        NULL, &graph);
    graph.stream_filter.input.indexed = &image;
    graph.stream_filter.output.receipt = &receipt;
    graph.stream_filter.flags = run.rows_ready != NULL
        ? SIXEL_FILTER_FLAG_PIPELINED : 0;
    status = sixel_filter_run(&graph.stream_filter, dither->allocator, logger);
    if (graph.applied) {
        ++dither->apply_count;
        dither->last_apply_status = graph.apply_status;
        if (graph.apply_status == SIXEL_OK) ++dither->apply_success_count;
    }
    if (status != SIXEL_OK) goto end;
    if (session != NULL) {
        (void)sixel_filter_init(&graph.commit_filter, "retained-commit",
            SIXEL_FILTER_KIND_RETAINED_COMMIT, sixel_indexed_commit_filter,
            NULL, &graph);
        graph.commit_filter.input.indexed = &image;
        graph.commit_filter.input.retained = &read;
        graph.commit_filter.input.receipt = &receipt;
        status = sixel_filter_run(&graph.commit_filter, dither->allocator,
            logger);
        if (status != SIXEL_OK) goto end;
        session->update = update;
        update = NULL;
    }
    ++output->indexed_count;
end:
    if (graph.consumed) (void)output->core->vtbl->abort(output->core);
    if (graph.opened) (void)output->writer->vtbl->abort_image(output->writer);
    sixel_retained_read_unref(read);
    sixel_retained_update_unref(update);
    sixel_stream_receipt_unref(receipt);
    sixel_indexed_frame_unref(image);
    if (emit_started > 0 && status == SIXEL_OK) {
        prepared->stages.emit_seconds = sixel_timer_monotonic() - emit_started;
        prepared->stages.valid |= SIXEL_PLAYBACK_STAGE_EMIT;
    }
    sixel_output_dispose_deferred(&local);
    dither->pipeline_logger = saved_logger;
    if (saved_logger == NULL) sixel_timeline_logger_unref(logger);
    return status;
}
