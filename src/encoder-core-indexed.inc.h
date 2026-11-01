/* SPDX-License-Identifier: MIT */
/* Included by encoder-core-encode.c to share its band algorithms. This
 * component owns index consumption and ordered byte production; its scratch
 * uses no SIXEL writer, terminal framing, lookup or retained-plane state. */

typedef struct sixel_indexed_core {
    sixel_encoder_core_t interface;
    sixel_atomic_u32_t ref;
    sixel_allocator_t *allocator;
    sixel_indexed_frame_t *image;
    sixel_output_t *body;
    sixel_encoder_core_options_t options;
    sixel_encode_work_t work;
    int rows;
    int band;
    int palette_sent;
    int force_ormode;
    SIXELSTATUS status;
#if SIXEL_ENABLE_THREADS
    sixel_parallel_context_t parallel;
    int parallel_active;
    int worker_capacity;
#endif
} sixel_indexed_core_t;

static void
sixel_indexed_core_ref(sixel_encoder_core_t *self)
{
    sixel_indexed_core_t *core;

    core = (sixel_indexed_core_t *)self;
    if (core != NULL) {
        (void)sixel_atomic_fetch_add_u32(&core->ref, 1);
    }
}

static SIXELSTATUS
sixel_indexed_core_release(sixel_indexed_core_t *core, int aborting)
{
    sixel_node_t *node;
    SIXELSTATUS status;
#if SIXEL_ENABLE_THREADS
    SIXELSTATUS wait_status;
#endif

    status = core->status;
#if SIXEL_ENABLE_THREADS
    if (core->parallel_active) {
        wait_status = sixel_parallel_context_wait(&core->parallel, aborting);
        if (status == SIXEL_OK) status = wait_status;
        sixel_parallel_context_cleanup(&core->parallel);
        core->parallel_active = 0;
    }
#else
    (void)aborting;
#endif
    if (core->body != NULL) {
        /* A failed compose can leave nodes in either list. */
        while ((node = core->body->node_top) != NULL) {
            core->body->node_top = node->next;
            sixel_allocator_free(core->allocator, node);
        }
        while ((node = core->body->node_free) != NULL) {
            core->body->node_free = node->next;
            sixel_allocator_free(core->allocator, node);
        }
        core->body->transparent_mask = NULL;
        core->body->wire_colors = NULL;
        memset(&core->body->body_sink, 0, sizeof(core->body->body_sink));
    }
    sixel_encode_work_cleanup(&core->work, core->allocator);
    if (core->image != NULL) {
        sixel_indexed_frame_release_user(core->image);
        sixel_indexed_frame_unref(core->image);
        core->image = NULL;
    }
    return status;
}

static void
sixel_indexed_core_unref(sixel_encoder_core_t *self)
{
    sixel_indexed_core_t *core;
    sixel_allocator_t *allocator;

    core = (sixel_indexed_core_t *)self;
    if (core == NULL || sixel_atomic_fetch_sub_u32(&core->ref, 1) != 1) {
        return;
    }
    allocator = core->allocator;
    (void)sixel_indexed_core_release(core, 1);
    sixel_output_storage_unref(core->body);
    sixel_allocator_free(allocator, core);
    sixel_allocator_unref(allocator);
}

static SIXELSTATUS
sixel_indexed_core_prepare(sixel_encoder_core_t *self,
    sixel_encoder_core_prepare_request_t const *request)
{
    sixel_indexed_core_t *core;
    sixel_indexed_frame_description_t const *desc;
    sixel_wire_palette_view_t palette;
    size_t i;
    SIXELSTATUS status;

    if (self == NULL || request == NULL || request->image == NULL ||
            request->sink.write == NULL || request->threads < 1 ||
            request->queue_bands < 1 || request->worker_capacity < 0 ||
            (request->worker_capacity != 0 &&
             request->worker_capacity < request->threads) ||
            request->options.encode_policy < SIXEL_ENCODEPOLICY_AUTO ||
            request->options.encode_policy > SIXEL_ENCODEPOLICY_SIZE) {
        return SIXEL_BAD_ARGUMENT;
    }
    core = (sixel_indexed_core_t *)self;
    if (core->image != NULL) {
        return SIXEL_LOGIC_ERROR;
    }
    if (sixel_indexed_frame_get_state(request->image) == SIXEL_INDEXED_FAILED) {
        return SIXEL_BAD_ARGUMENT;
    }
    desc = &request->image->description;
    sixel_wire_palette_get_view(desc->palette, &palette);
    if (request->options.ormode || core->force_ormode) {
        /* OR bitplanes designate register bits, so arbitrary register
         * remapping and retained holes cannot be represented by this mode. */
        if (desc->transparent_background || desc->keycolor >= 0) {
            return SIXEL_BAD_ARGUMENT;
        }
        for (i = 0; i < palette.count; ++i) {
            if (palette.colors[i].definition.register_index != i) {
                return SIXEL_BAD_ARGUMENT;
            }
        }
    }
    core->options = request->options;
    core->options.ormode |= core->force_ormode;
    core->rows = 0;
    core->band = 0;
    core->palette_sent = 0;
    core->status = SIXEL_OK;
    core->body->body_status = SIXEL_OK;
    core->body->body_sink = request->sink;
    core->body->pos = 0;
    core->body->save_count = 0;
    core->body->save_pixel = 0;
    core->body->active_palette = -1;
    core->body->has_gri_arg_limit = !!request->options.has_gri_arg_limit;
    core->body->encode_policy = request->options.encode_policy;
    core->body->ormode = core->options.ormode;
    core->body->keycolor_reserved = desc->keycolor_reserved;
    core->body->transparent_offset_left = desc->encoded_x;
    core->body->transparent_offset_top = desc->encoded_y;
    core->body->encoded_width = desc->encoded_width;
    core->body->encoded_height = desc->encoded_height;
    core->body->transparent_mask = request->image->skip;
    core->body->wire_colors = palette.colors;
    core->image = request->image;
    sixel_indexed_frame_ref(core->image);
    sixel_indexed_frame_acquire_user(core->image);
    status = SIXEL_OK;
#if SIXEL_ENABLE_THREADS
    core->worker_capacity = request->worker_capacity != 0
        ? request->worker_capacity : request->threads;
    sixel_parallel_context_init(&core->parallel);
    if (request->threads > 1 && desc->encoded_height > 6) {
        status = sixel_parallel_context_begin(&core->parallel,
            core->image->indexes, desc->width, desc->height,
            (int)palette.count, desc->keycolor, NULL, core->body,
            core->allocator, request->threads, core->worker_capacity,
            request->queue_bands, 0, request->logger);
        if (SIXEL_FAILED(status)) {
            sixel_parallel_context_cleanup(&core->parallel);
        } else {
            core->parallel_active = 1;
        }
    } else
#endif
    if (!core->options.ormode) {
        status = sixel_encode_work_allocate(&core->work,
            desc->encoded_width, (int)palette.count, core->allocator);
    }
    if (SIXEL_FAILED(status)) {
        core->status = status;
        (void)sixel_indexed_core_release(core, 1);
    }
    return status;
}

static SIXELSTATUS
sixel_indexed_core_palette(sixel_indexed_core_t *core)
{
    sixel_wire_palette_view_t palette;
    sixel_wire_color_definition_t const *color;
    char bytes[80];
    size_t i;
    int length;
    SIXELSTATUS status;

    if (core->palette_sent) {
        return SIXEL_OK;
    }
    core->palette_sent = 1;
    if (!core->options.emit_palette) {
        return SIXEL_OK;
    }
    sixel_wire_palette_get_view(core->image->description.palette, &palette);
    for (i = 0; i < palette.count; ++i) {
        if ((int)i == core->image->description.keycolor) {
            continue;
        }
        color = &palette.colors[i].definition;
        length = snprintf(bytes, sizeof(bytes), "#%u;%d;%d;%d;%d",
            color->register_index, color->type, color->fields[0],
            color->fields[1], color->fields[2]);
        if (length < 0 || (size_t)length >= sizeof(bytes)) {
            return SIXEL_LOGIC_ERROR;
        }
        status = sixel_output_write_bytes(core->body, bytes, length);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_indexed_core_consume(sixel_encoder_core_t *self, int published_rows)
{
    sixel_indexed_core_t *core;
    sixel_indexed_frame_description_t const *desc;
    sixel_wire_palette_view_t palette;
    sixel_indexed_rows_view_t rows;
    sixel_band_state_t band;
    int start;
    int height;
    int ready;
    SIXELSTATUS status;

    if (self == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    core = (sixel_indexed_core_t *)self;
    if (core->image == NULL || published_rows < core->rows ||
            SIXEL_FAILED(core->status)) {
        return SIXEL_LOGIC_ERROR;
    }
    status = sixel_indexed_frame_get_rows(core->image, 0,
        published_rows, &rows);
    if (status != SIXEL_OK) {
        return status;
    }
    desc = &core->image->description;
    sixel_wire_palette_get_view(desc->palette, &palette);
    core->rows = published_rows;
    status = sixel_indexed_core_palette(core);
    if (SIXEL_FAILED(status)) {
        core->status = status;
        return status;
    }
    ready = published_rows == desc->height ? desc->encoded_height :
        desc->encoded_y + published_rows;
    while (core->band < sixel_count_sixel_bands(desc->encoded_height)) {
        start = core->band * 6;
        height = desc->encoded_height - start;
        if (height > 6) {
            height = 6;
        }
        if (start + height > ready) {
            break;
        }
#if SIXEL_ENABLE_THREADS
        if (core->parallel_active) {
            sixel_parallel_submit_band(&core->parallel, core->band);
            if (!sixel_parallel_jobs_allowed(&core->parallel)) {
                status = SIXEL_RUNTIME_ERROR;
                break;
            }
        } else
#endif
        if (core->options.ormode) {
            status = sixel_encode_body_ormode_band_offset(core->image->indexes,
                desc->width, desc->height, core->band,
                sixel_encode_body_ormode_nplanes((int)palette.count),
                desc->encoded_width, desc->encoded_height,
                desc->encoded_x, desc->encoded_y, core->body);
        } else {
            sixel_band_state_reset(&band);
            sixel_band_classify(&core->work, &band, core->image->indexes,
                desc->width, desc->height, desc->encoded_width, start, height,
                desc->encoded_x, desc->encoded_y, (int)palette.count,
                desc->keycolor, NULL, core->options.encode_policy,
                core->image->skip);
            status = sixel_band_compose(&core->work, &band, core->body,
                desc->encoded_width, (int)palette.count, desc->keycolor,
                core->allocator);
            if (SIXEL_SUCCEEDED(status)) {
                status = sixel_band_emit(&core->work, &band, core->body,
                    (int)palette.count, desc->keycolor, start + height - 1);
            }
            sixel_band_finish(&core->work, &band, desc->encoded_width);
        }
        if (SIXEL_FAILED(status)) {
            break;
        }
        ++core->band;
    }
    core->status = SIXEL_FAILED(status) ? status : core->body->body_status;
    return core->status;
}

static SIXELSTATUS
sixel_indexed_core_finish(sixel_encoder_core_t *self)
{
    sixel_indexed_core_t *core;
    SIXELSTATUS status;

    if (self == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    core = (sixel_indexed_core_t *)self;
    if (core->image == NULL || SIXEL_FAILED(core->status) ||
            sixel_indexed_frame_get_state(core->image) !=
                SIXEL_INDEXED_COMPLETE ||
            core->rows != core->image->description.height) {
        return SIXEL_LOGIC_ERROR;
    }
#if SIXEL_ENABLE_THREADS
    if (core->parallel_active) {
        /* COMPLETE seals all producer writes; its workers have joined before
         * the session calls finish, so their budget can drain the body tail. */
        status = sixel_parallel_context_grow(&core->parallel,
            core->worker_capacity);
        if (status != SIXEL_OK) return status;
        status = sixel_parallel_context_wait(&core->parallel, 0);
        sixel_parallel_context_cleanup(&core->parallel);
        core->parallel_active = 0;
        if (SIXEL_FAILED(status)) {
            core->status = status;
            return status;
        }
    }
#endif
    status = core->body->body_status;
    if (SIXEL_SUCCEEDED(status) && core->body->pos > 0) {
        status = sixel_output_write_bytes(core->body,
            (char *)core->body->buffer, core->body->pos);
        core->body->pos = 0;
    }
    core->status = status;
    if (SIXEL_SUCCEEDED(status)) {
        (void)sixel_indexed_core_release(core, 0);
    }
    return status;
}

static SIXELSTATUS
sixel_indexed_core_abort(sixel_encoder_core_t *self)
{
    if (self == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    (void)sixel_indexed_core_release((sixel_indexed_core_t *)self, 1);
    return SIXEL_OK;
}

static sixel_encoder_core_vtbl_t const g_indexed_core_vtbl = {
    sixel_indexed_core_ref,
    sixel_indexed_core_unref,
    sixel_indexed_core_prepare,
    sixel_indexed_core_consume,
    sixel_indexed_core_finish,
    sixel_indexed_core_abort
};

SIXEL_INTERNAL_API SIXELSTATUS
sixel_encoder_core_factory_new(sixel_allocator_t *allocator, void **object)
{
    sixel_indexed_core_t *core;
    SIXELSTATUS status;

    if (allocator == NULL || object == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *object = NULL;
    core = sixel_allocator_calloc(allocator, 1, sizeof(*core));
    if (core == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    core->interface.vtbl = &g_indexed_core_vtbl;
    core->ref = 1;
    core->allocator = allocator;
    sixel_encode_work_init(&core->work);
    status = sixel_output_storage_new(allocator, &core->body, 0);
    if (SIXEL_FAILED(status)) {
        sixel_allocator_free(allocator, core);
        return status;
    }
    sixel_allocator_ref(allocator);
    *object = &core->interface;
    return SIXEL_OK;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_encoder_core_normal_factory_new(sixel_allocator_t *allocator,
    void **object)
{
    return sixel_encoder_core_factory_new(allocator, object);
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_encoder_core_ormode_factory_new(sixel_allocator_t *allocator,
    void **object)
{
    SIXELSTATUS status;

    status = sixel_encoder_core_factory_new(allocator, object);
    if (SIXEL_SUCCEEDED(status)) {
        ((sixel_indexed_core_t *)*object)->force_ormode = 1;
    }
    return status;
}
