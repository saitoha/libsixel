/* SPDX-License-Identifier: MIT */

/* Only the runtime knows the native allocation behind a gpu_buffer. */
typedef struct gpu_native_buffer {
    sixel_gpu_buffer_interface_t iface;
    unsigned int refs;
    id<MTLBuffer> native;
} gpu_native_buffer_t;

typedef struct gpu_compiled_program {
    NSString *source;
    NSArray *pipelines;
    unsigned int refs;
    struct gpu_compiled_program *next;
} gpu_compiled_program_t;

struct sixel_gpu_program {
    sixel_gpu_lookup_plan_interface_t *plan;
    sixel_gpu_lookup_program_view_t view;
    gpu_compiled_program_t *compiled;
    int legacy;
};

static gpu_compiled_program_t *gpu_compiled_cache;
static size_t gpu_compiled_count;

static void
gpu_buffer_ref(sixel_gpu_buffer_interface_t *self)
{
    gpu_native_buffer_t *buffer;

    buffer = (gpu_native_buffer_t *)self;
    (void)__atomic_add_fetch(&buffer->refs, 1U, __ATOMIC_RELAXED);
}

static void
gpu_buffer_unref(sixel_gpu_buffer_interface_t *self)
{
    gpu_native_buffer_t *buffer;

    buffer = (gpu_native_buffer_t *)self;
    if (__atomic_sub_fetch(&buffer->refs, 1U, __ATOMIC_ACQ_REL) != 0) return;
    [buffer->native release];
    free(buffer);
}

static size_t
gpu_buffer_size(sixel_gpu_buffer_interface_t const *self)
{
    gpu_native_buffer_t const *buffer;

    buffer = (gpu_native_buffer_t const *)self;
    return (size_t)[buffer->native length];
}

static sixel_gpu_buffer_vtbl_t const gpu_buffer_vtbl = {
    gpu_buffer_ref, gpu_buffer_unref, gpu_buffer_size
};

static void
gpu_device_ref(sixel_gpu_device_interface_t *self)
{
    /* The selected runtime device has process lifetime. */
    (void)self;
}

static char const *
gpu_device_format(sixel_gpu_device_interface_t const *self)
{
    (void)self;
    return "metal.lookup-rgb8.v1";
}

static SIXELSTATUS
gpu_device_buffer(sixel_gpu_device_interface_t *self, size_t bytes,
    sixel_gpu_buffer_interface_t **result)
{
    gpu_native_buffer_t *buffer;

    (void)self;
    if (result == NULL || bytes == 0) return SIXEL_BAD_ARGUMENT;
    *result = NULL;
    if (bytes > [g_sixel_gpu_metal_device maxBufferLength]) {
        return SIXEL_BAD_ARGUMENT;
    }
    buffer = calloc(1, sizeof(*buffer));
    if (buffer == NULL) return SIXEL_BAD_ALLOCATION;
    buffer->native = [g_sixel_gpu_metal_device newBufferWithLength:bytes
        options:MTLResourceStorageModeShared];
    if (buffer->native == nil) {
        free(buffer);
        return SIXEL_BAD_ALLOCATION;
    }
    buffer->iface.vtbl = &gpu_buffer_vtbl;
    buffer->refs = 1;
    *result = &buffer->iface;
    return SIXEL_OK;
}

static sixel_gpu_device_vtbl_t const gpu_device_vtbl = {
    gpu_device_ref, gpu_device_ref, gpu_device_format, gpu_device_buffer
};
static sixel_gpu_device_interface_t gpu_device = { &gpu_device_vtbl };

SIXELSTATUS
sixel_gpu_device_acquire(sixel_gpu_device_interface_t **result)
{
    int available;

    if (result == NULL) return SIXEL_BAD_ARGUMENT;
    *result = NULL;
    (void)pthread_mutex_lock(&g_sixel_gpu_metal_lock);
    available = sixel_gpu_palette_metal_prepare_device_locked();
    (void)pthread_mutex_unlock(&g_sixel_gpu_metal_lock);
    if (!available) return SIXEL_FEATURE_ERROR;
    *result = &gpu_device;
    return SIXEL_OK;
}

static void
gpu_compiled_unref(gpu_compiled_program_t *compiled)
{
    if (--compiled->refs != 0) return;
    [compiled->source release];
    [compiled->pipelines release];
    free(compiled);
}

static SIXELSTATUS
gpu_compile(sixel_gpu_lookup_program_view_t const *view,
    gpu_compiled_program_t **result)
{
    NSMutableString *source;
    NSMutableArray *pipelines;
    NSString *entry;
    id<MTLLibrary> library;
    id<MTLFunction> function;
    id<MTLComputePipelineState> pipeline;
    gpu_compiled_program_t *compiled;
    gpu_compiled_program_t *evicted;
    NSError *error;
    size_t i;
    size_t j;
    size_t threads;
    MTLSize limits;
    SIXELSTATUS status;

    *result = NULL;
    source = [[NSMutableString alloc] init];
    pipelines = [[NSMutableArray alloc] init];
    library = nil;
    status = SIXEL_FEATURE_ERROR;
    for (i = 0; i < sizeof(fused_prefix) / sizeof(*fused_prefix); ++i) {
        [source appendString:@(fused_prefix[i])];
    }
    for (i = 0; i < view->chunk_count; ++i) {
        if (view->chunks[i] == NULL) goto end;
        [source appendString:@(view->chunks[i])];
    }
    for (i = 0; i < sizeof(fused_suffix) / sizeof(*fused_suffix); ++i) {
        [source appendString:@(fused_suffix[i])];
    }
    /* Full source and builder entry order, not a policy name, identify code.
     * Geometry is validated for every plan, even on a compiled cache hit. */
    for (i = 0; i < view->build_pass_count; ++i) {
        if (view->build_passes[i].entry_point == NULL) goto end;
        [source appendFormat:@"\n// entry:%s\n",
            view->build_passes[i].entry_point];
    }
    compiled = gpu_compiled_cache;
    while (compiled != NULL && ![compiled->source isEqualToString:source]) {
        compiled = compiled->next;
    }
    if (compiled == NULL) {
        error = nil;
        library = [g_sixel_gpu_metal_device newLibraryWithSource:source
            options:nil error:&error];
        if (library == nil) {
            sixel_gpu_palette_metal_set_message("GPU lookup program", error);
            goto end;
        }
        for (i = 0; i <= view->build_pass_count; ++i) {
            entry = i == view->build_pass_count ? @"sixel_gpu_fused_lookup"
                : @(view->build_passes[i].entry_point);
            function = [library newFunctionWithName:entry];
            if (function == nil) goto end;
            pipeline = [g_sixel_gpu_metal_device
                newComputePipelineStateWithFunction:function error:&error];
            [function release];
            if (pipeline == nil) goto end;
            [pipelines addObject:pipeline];
            [pipeline release];
        }
        compiled = calloc(1, sizeof(*compiled));
        if (compiled == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        compiled->source = [source copy];
        compiled->pipelines = [pipelines copy];
        compiled->refs = 1;
        /* Bounded code retention; live executions keep evicted code alive. */
        if (gpu_compiled_count == 8) {
            evicted = gpu_compiled_cache;
            gpu_compiled_cache = evicted->next;
            gpu_compiled_unref(evicted);
            --gpu_compiled_count;
        }
        compiled->next = gpu_compiled_cache;
        gpu_compiled_cache = compiled;
        ++gpu_compiled_count;
    }
    limits = [g_sixel_gpu_metal_device maxThreadsPerThreadgroup];
    for (i = 0; i < view->build_pass_count; ++i) {
        threads = 1;
        for (j = 0; j < 3; ++j) {
            if (view->build_passes[i].groups[j] == 0 ||
                    view->build_passes[i].threads[j] == 0 ||
                    view->build_passes[i].threads[j] > SIZE_MAX / threads) {
                goto end;
            }
            threads *= view->build_passes[i].threads[j];
        }
        pipeline = compiled->pipelines[i];
        if (threads > [pipeline maxTotalThreadsPerThreadgroup] ||
                view->build_passes[i].threads[0] > limits.width ||
                view->build_passes[i].threads[1] > limits.height ||
                view->build_passes[i].threads[2] > limits.depth) goto end;
    }
    ++compiled->refs;
    *result = compiled;
    status = SIXEL_OK;
end:
    [library release];
    [pipelines release];
    [source release];
    return status;
}

SIXELSTATUS
sixel_gpu_program_prepare(sixel_gpu_device_interface_t *device,
    sixel_gpu_lookup_plan_interface_t *plan, sixel_gpu_program_t **result)
{
    sixel_gpu_program_t *program;
    SIXELSTATUS status;

    if (device == NULL || plan == NULL || result == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *result = NULL;
    if (device != &gpu_device) return SIXEL_FEATURE_ERROR;
    program = calloc(1, sizeof(*program));
    if (program == NULL) return SIXEL_BAD_ALLOCATION;
    status = plan->vtbl->describe(plan, &program->view);
    if (SIXEL_FAILED(status)) goto end;
    if (program->view.program_format == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    program->legacy = strcmp(program->view.program_format,
        "libsixel.legacy-palette.v1") == 0;
    (void)pthread_mutex_lock(&g_sixel_gpu_metal_lock);
    if (program->legacy) {
        status = sixel_gpu_palette_metal_prepare_locked()
            ? SIXEL_OK : SIXEL_FEATURE_ERROR;
    } else if (strcmp(program->view.program_format,
            "metal.lookup-rgb8.v1") != 0) {
        status = SIXEL_FEATURE_ERROR;
    } else if (program->view.palette == NULL || program->view.ncolors < 1 ||
            program->view.ncolors > 256 || program->view.workspace_size == 0 ||
            program->view.workspace_size >
                [g_sixel_gpu_metal_device maxBufferLength] ||
            program->view.constants_size >
                [g_sixel_gpu_metal_device maxBufferLength] ||
            program->view.palette_size < program->view.ncolors * 3U ||
            program->view.chunks == NULL || program->view.chunk_count == 0 ||
            (program->view.constants_size != 0 &&
             program->view.constants == NULL) ||
            (program->view.build_pass_count != 0 &&
             program->view.build_passes == NULL)) {
        status = SIXEL_BAD_ARGUMENT;
    } else {
        status = gpu_compile(&program->view, &program->compiled);
    }
    (void)pthread_mutex_unlock(&g_sixel_gpu_metal_lock);
end:
    if (SIXEL_FAILED(status)) {
        free(program);
        return status;
    }
    program->plan = plan;
    plan->vtbl->ref(plan);
    *result = program;
    return SIXEL_OK;
}

void
sixel_gpu_program_destroy(sixel_gpu_program_t *program)
{
    if (program == NULL) return;
    program->plan->vtbl->unref(program->plan);
    (void)pthread_mutex_lock(&g_sixel_gpu_metal_lock);
    if (program->compiled != NULL) gpu_compiled_unref(program->compiled);
    (void)pthread_mutex_unlock(&g_sixel_gpu_metal_lock);
    free(program);
}
