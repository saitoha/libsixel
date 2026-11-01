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
    if (graph->session != NULL) graph->session->partial_output = 1;
    status = graph->writer->vtbl->begin_image(graph->writer, &graph->header,
        &image->identity);
    if (status != SIXEL_OK) return status;
    if (graph->run->rows_ready != NULL) {
        sixel_timeline_logger_logf(logger, "controller", "encode",
            "body_start", -1);
        body_started = 1;
    }
    status = sixel_filter_run(&graph->apply_filter, allocator, logger);
    if (status != SIXEL_OK) goto end;
    if (graph->run->rows_ready == NULL) {
        sixel_timeline_logger_logf(logger, "controller", "encode",
            "body_start", -1);
        body_started = 1;
        status = graph->core->vtbl->consume(graph->core,
            image->description.height);
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
     * the session read lease before the exclusive, allocation-free loop. */
    sixel_retained_read_unref(*filter->input.retained);
    *filter->input.retained = NULL;
    sixel_timeline_logger_logf(logger, "controller", "retained_commit",
        "start", -1);
    status = graph->session->plane->vtbl->commit(graph->session->plane,
        graph->update, *filter->input.receipt, &graph->session->stats);
    sixel_timeline_logger_logf(logger, "controller", "retained_commit",
        "finish", -1);
    if (status == SIXEL_OK) {
        graph->session->committed = 1;
        graph->session->partial_output = 0;
    }
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
    sixel_frame_t *source;
    sixel_wire_palette_t *wire;
    sixel_indexed_frame_t *image;
    sixel_retained_read_t *read;
    sixel_retained_update_t *update;
    sixel_stream_receipt_t *receipt;
    sixel_palette_entries_view_t entries;
    sixel_wire_color_definition_t definitions[SIXEL_PALETTE_MAX];
    unsigned char colors[SIXEL_PALETTE_MAX * 3];
    sixel_wire_palette_request_t wire_request;
    sixel_indexed_frame_description_t description;
    sixel_dither_policy_prepare_request_t configuration;
    sixel_palette_apply_prepare_request_t prepare;
    sixel_palette_apply_run_request_t run;
    sixel_encoder_core_prepare_request_t body;
    sixel_writer_image_header_t header;
    sixel_retained_read_view_t baseline;
    sixel_retained_update_request_t update_request;
    sixel_dither_execution_t execution;
    sixel_dither_encode_allocation_t allocation;
    sixel_indexed_subgraph_t graph;
    double rgb[3];
    void *object;
    size_t pixels;
    size_t i;
    int channel;
    int threads;
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

    source = NULL;
    wire = NULL;
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
    status = sixel_dither_prepare_indexed_components(dither, threads > 1);
    if (status != SIXEL_OK) goto end;
    pixels = (size_t)request->width * (size_t)request->height;
    memset(&wire_request, 0, sizeof(wire_request));
    memset(&description, 0, sizeof(description));
    memset(&prepare, 0, sizeof(prepare));
    memset(&run, 0, sizeof(run));
    memset(&body, 0, sizeof(body));
    memset(&header, 0, sizeof(header));
    memset(&update_request, 0, sizeof(update_request));
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
    status = sixel_indexed_frame_new(&image, &description, dither->allocator);
    if (SIXEL_FAILED(status)) goto end;
    status = sixel_frame_new(&source, dither->allocator);
    if (SIXEL_FAILED(status)) goto end;
    status = sixel_frame_init_borrowed(source, request->pixels,
        request->width, request->height, SIXEL_PIXELFORMAT_RGB888, NULL, -1);
    if (SIXEL_FAILED(status)) goto end;
    status = dither->dither_policy->vtbl->get_configuration(
        dither->dither_policy, &configuration);
    if (SIXEL_FAILED(status)) goto end;
#if SIXEL_ENABLE_THREADS
    sixel_parallel_dither_configure(request->height, threads, pixels,
        dither, &allocation);
#else
    (void)pixels;
    memset(&allocation, 0, sizeof(allocation));
    allocation.mode = SIXEL_DITHER_ENCODE_SERIAL;
    allocation.dither_threads = allocation.encode_threads = 1;
#endif
    prepare.working_palette = dither->palette;
    prepare.lookup = dither->lookup_policy;
    prepare.dither = dither->dither_policy;
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
    object = NULL;
    status = sixel_palette_apply_factory_new(dither->allocator, &object);
    if (SIXEL_FAILED(status)) goto end;
    apply = object;
    status = apply->vtbl->prepare(apply, &prepare);
    if (status == SIXEL_FEATURE_ERROR) {
        /* Unsupported plans keep the legacy admission/framing behavior. */
        status = SIXEL_FALSE;
    }
    if (status != SIXEL_OK) goto end;
    if (session != NULL) {
        status = session->plane->vtbl->acquire_read(session->plane, &read);
        if (SIXEL_FAILED(status)) goto end;
        (void)sixel_retained_read_get_view(read, &baseline);
        if (baseline.sequence == UINT64_MAX) {
            status = SIXEL_BAD_INTEGER_OVERFLOW;
            goto end;
        }
        update_request.baseline = read;
        update_request.image = image;
        update_request.sequence = baseline.sequence + 1;
        update_request.record_paint = session->record_paint;
        status = session->plane->vtbl->begin_update(session->plane,
            &update_request, &update);
        if (SIXEL_FAILED(status)) goto end;
    }
    if (output->core == NULL) {
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
    status = output->core->vtbl->prepare(output->core, &body);
    if (SIXEL_FAILED(status)) goto end;
    graph.consumed = 1;
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
    run.delta.selection = execution.selection;
    if (allocation.mode == SIXEL_DITHER_ENCODE_PIPELINE) {
        run.rows_ready = sixel_session_rows;
        run.rows_context = output->core;
    }
    dither->pipeline_last_mode = allocation.mode;
    dither->pipeline_last_band_height = allocation.band_height;
    dither->pipeline_last_band_overlap = allocation.overlap;
    dither->pipeline_last_dither_threads = allocation.dither_threads;
    dither->pipeline_last_encode_threads = allocation.encode_threads;
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
    sixel_wire_palette_unref(wire);
    sixel_frame_unref(source);
    if (apply != NULL) apply->vtbl->unref(apply);
    dither->pipeline_logger = saved_logger;
    if (saved_logger == NULL) sixel_timeline_logger_unref(logger);
    return status;
}
