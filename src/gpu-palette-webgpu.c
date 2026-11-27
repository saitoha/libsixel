/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if defined(HAVE_WEBGPU)
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <emscripten.h>
#include "gpu-palette.h"
#include "gpu-statistics.h"

static char const *const palette_point_main[] = {
#include <gpu-palette-point-wgsl.inc.h>
};
static char const *const palette_batch_main[] = {
#include <gpu-palette-batch-wgsl.inc.h>
};

/* Keep these scalar words in the same order as WGSL Params. */
typedef struct webgpu_params {
    uint32_t count, first, width;
    float strength;
    uint32_t save_query, phase_x, phase_y, channel_rgb;
    float gradient_factor;
    uint32_t gradient_size, gradient_width, gradient_height;
} webgpu_params_t;

struct sixel_gpu_program {
    sixel_gpu_lookup_plan_interface_t *lookup;
    sixel_gpu_lookup_program_view_t view;
    sixel_gpu_dither_program_t const *dither;
};

/* Emscripten may execute a prepared frame on another worker. Each JS realm
 * owns its own device and pipeline cache. Only immutable shader descriptors
 * cross the C prepare/run boundary. Never hold a Wasm heap view over await. */
EM_JS(int, webgpu_device_state, (void), {
    if (!globalThis.navigator?.gpu) return -1;
    return Module['sixelWebGPU']?.paletteBuffers ? 1 : 0;
})

EM_ASYNC_JS(int, webgpu_open, (void), {
    try {
        const adapter = await navigator.gpu.requestAdapter();
        if (!adapter) return 0;
        const device = await adapter.requestDevice();
        const state = {device};
        // WebGPU copies whole words. Pad only the last word and never
        // keep a typed view of the growable Wasm heap across an await.
        state.writeBytes = (buffer, at, ptr, size) => {
            const bulk = size - size % 4;
            if (bulk)
                device.queue.writeBuffer(buffer, at, HEAPU8, ptr >>> 0, bulk);
            if (bulk < size) {
                const tail = new Uint8Array(4);
                const offset = (ptr >>> 0) + bulk;
                tail.set(HEAPU8.subarray(offset, offset + size - bulk));
                device.queue.writeBuffer(buffer, at + bulk, tail);
            }
        };
        function chunks(address, length) {
            let code = "";
            for (let i = 0; i < length; ++i)
                code += UTF8ToString(HEAPU32[(address >>> 2) + i]);
            return code;
        }
        state.chunks = chunks;
        state.getPipeline = async (lookup, nlookup, dither, ndither,
                                   entry, ne) => {
            const code = chunks(lookup, nlookup) + chunks(dither, ndither) +
                         chunks(entry, ne);
            if (state.code === code) return state.pipeline;
            const module = device.createShaderModule({code});
            const pipeline = await device.createComputePipelineAsync({
                layout: 'auto', compute: {module, entryPoint: 'main'}
            });
            state.code = code;
            state.pipeline = pipeline;
            return pipeline;
        };
        Module['sixelWebGPU'] = state;
        device.lost.then(() => {
            if (Module['sixelWebGPU'] === state) delete Module['sixelWebGPU'];
        });
        return 1;
    } catch (error) {
        return 0;
    }
})

typedef char webgpu_build_pass_words[
    sizeof(sixel_gpu_lookup_build_pass_t) == 7U * sizeof(uint32_t) &&
    offsetof(sixel_gpu_lookup_build_pass_t, groups) == sizeof(uint32_t)
    ? 1 : -1];

/* Tables are device-local and immutable after their last builder pass.
 * A worker can reuse a table only with identical shader, palette, constants
 * and dispatch geometry. Other live calls retain their own buffer handle. */
EM_JS(void, webgpu_install_tables, (void), {
    const state = Module['sixelWebGPU'], device = state.device;
    state.table = async (chunks, nc, pp, colors, cp, cb, size, bp, count) => {
        if (!size) return null;
        if (size > device.limits.maxStorageBufferBindingSize ||
            size > device.limits.maxBufferSize) throw Error('lookup capacity');
        const passes = [];
        for (let i = 0; i < count; ++i) {
            const at = (bp >>> 2) + i * 7;
            passes.push([UTF8ToString(HEAPU32[at]), HEAPU32[at + 1],
                HEAPU32[at + 2], HEAPU32[at + 3]]);
        }
        const code = state.chunks(chunks, nc);
        const key = JSON.stringify([size, passes,
            Array.from(HEAPU8.subarray(pp >>> 0, (pp >>> 0) + 3 * colors)),
            Array.from(HEAPU8.subarray(cp >>> 0, (cp >>> 0) + cb))]);
        const prev = state.lookupTable;
        function lease(owner) {
            ++owner.users;
            return {buffer:owner.table,release:() => {
                if (--owner.users === 0 && state.lookupTable !== owner)
                    owner.table.destroy();
            }};
        }
        if (prev && code === prev.code && key === prev.key) return lease(prev);
        const names = passes.map(p => p[0]).join(',');
        let builders = state.builders;
        if (!builders || code !== builders.code || names !== builders.names) {
            const entries = [1,7,8].map(binding => ({binding,
                visibility: GPUShaderStage.COMPUTE,
                buffer:{type:binding === 8 ? 'storage' : 'read-only-storage'}
            }));
            const layout = device.createBindGroupLayout({entries});
            const pl = device.createPipelineLayout({bindGroupLayouts:[layout]});
            const module = device.createShaderModule({code});
            const pipes = await Promise.all(passes.map(([entryPoint]) =>
                device.createComputePipelineAsync({layout:pl,
                    compute:{module, entryPoint}})));
            state.builders = builders = {code,names,entries,layout,pipes};
        }
        const {entries,layout,pipes} = builders;
        const usage = GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST;
        let pal, constants, table, valid, oom;
        device.pushErrorScope('out-of-memory');
        device.pushErrorScope('validation');
        try {
            pal = device.createBuffer({size:4 * colors, usage});
            constants = device.createBuffer({size:Math.max(cb,4), usage});
            table = device.createBuffer({size, usage:GPUBufferUsage.STORAGE});
            const packed = new Uint32Array(colors);
            for (let i = 0; i < colors; ++i) {
                const p = (pp >>> 0) + 3 * i;
                packed[i] = HEAPU8[p] | HEAPU8[p+1] << 8 | HEAPU8[p+2] << 16;
            }
            device.queue.writeBuffer(pal, 0, packed);
            if (cb) state.writeBytes(constants, 0, cp, cb);
            const group = device.createBindGroup({layout, entries:
                [pal,constants,table].map((buffer,i) =>
                    ({binding:entries[i].binding,resource:{buffer}}))});
            const enc = device.createCommandEncoder();
            passes.forEach((p,i) => {
                const pass = enc.beginComputePass();
                pass.setPipeline(pipes[i]);
                pass.setBindGroup(0, group);
                pass.dispatchWorkgroups(p[1],p[2],p[3]);
                pass.end();
            });
            device.queue.submit([enc.finish()]);
        } catch (error) {
            table?.destroy();
            throw error;
        } finally {
            valid = device.popErrorScope();
            oom = device.popErrorScope();
            pal?.destroy();
            constants?.destroy();
        }
        const errors = await Promise.all([valid,oom]);
        if (errors.some(Boolean)) {
            table.destroy();
            throw Error('lookup builder failed');
        }
        const old = state.lookupTable;
        if (old && !old.users) old.table.destroy();
        state.lookupTable = {code,key,table,users:0};
        return lease(state.lookupTable);
    };
})

/* Pass the caller's list by reference so partially allocated buffers are
 * destroyed even if allocation throws before the full set is ready. */
EM_JS(void, webgpu_install_buffers, (void), {
    const state = Module['sixelWebGPU'], device = state.device;
    state.paletteBuffers = (buffers, count, colors, corrected,
                            noiseBytes, nbytes) => {
        function buffer(size, usage) {
            const b = device.createBuffer({size, usage});
            buffers.push(b);
        }
        const U = GPUBufferUsage;
        const length = Math.ceil(count / 4) * 4;
        // Lease the previous frame's buffers when the binding sizes match.
        // A concurrent call cannot reuse them until mapAsync has completed.
        if (state.idle) {
            if (state.idle.count === count && state.idle.colors === colors &&
                    state.idle.saveQuery === !!corrected &&
                    state.idle.noiseBytes === noiseBytes &&
                    state.idle.nbytes === nbytes)
                buffers.push(...state.idle.buffers);
            else
                for (const b of state.idle.buffers) b.destroy();
            state.idle = null;
        }
        if (!buffers.length) {
            buffer(Math.ceil(3 * count / 4) * 4, U.STORAGE | U.COPY_DST);
            buffer(4 * colors, U.STORAGE | U.COPY_DST);
            buffer(length, U.STORAGE | U.COPY_SRC | U.COPY_DST);
            buffer(48, U.UNIFORM | U.COPY_DST);
            buffer(4, U.STORAGE | U.COPY_SRC | U.COPY_DST);
            buffer(corrected ? count * 4 : 4, U.STORAGE | U.COPY_SRC);
            buffer(length + 4 + (corrected ? count * 4 : 0),
                   U.MAP_READ | U.COPY_DST);
            if (noiseBytes) buffer(noiseBytes, U.STORAGE | U.COPY_DST);
            if (nbytes) buffer(nbytes, U.STORAGE | U.COPY_DST);
        }
    };
})

int
sixel_webgpu_available(void)
{
    int state;

    state = webgpu_device_state();
    if (state != 0) return state > 0;
    if (!webgpu_open()) return 0;
    webgpu_install_tables();
    webgpu_install_buffers();
    return 1;
}

EM_JS_DEPS(sixel_webgpu_deps, "$UTF8ToString")

#if !SIXEL_ENABLE_THREADS
/* Index planes live in the JS realm of the serial consumer. A plane holds
 * the frame's indexes and one invalid-index word per palette chunk, plus
 * the error scopes of the chunks that wrote it. Released planes of the
 * frame size are kept for the next frame. */
EM_JS(int, webgpu_plane_new, (int size), {
    const state = Module['sixelWebGPU'];
    if (!state) return 0;
    const device = state.device;
    if (!state.planes) state.planes = {map:new Map(), next:1, idle:[]};
    const planes = state.planes;
    const length = 4 * Math.ceil(size / 4);
    const words = 4 * Math.ceil(size / 1048576);
    if (length > device.limits.maxBufferSize ||
            length > device.limits.maxStorageBufferBindingSize) return 0;
    const at = planes.idle.findIndex(p => p.indexes.size === length);
    let plane = at < 0 ? null : planes.idle.splice(at, 1)[0];
    if (!plane) {
        const U = GPUBufferUsage;
        try {
            plane = {
                indexes:device.createBuffer({size:length,
                    usage:U.STORAGE | U.COPY_SRC | U.COPY_DST}),
                status:device.createBuffer({size:words,
                    usage:U.COPY_SRC | U.COPY_DST}),
                staging:null
            };
        } catch (error) {
            return 0;
        }
    }
    // device: the indexes buffer holds this frame. stale: only it does.
    plane.device = plane.stale = false;
    plane.pending = [];
    const id = planes.next++;
    planes.map.set(id, plane);
    return id;
})

EM_JS(void, webgpu_plane_release, (int id), {
    const planes = Module['sixelWebGPU']?.planes;
    const plane = planes?.map.get(id);
    if (!plane) return;
    planes.map.delete(id);
    if (planes.idle.length < 2) {
        planes.idle.push(plane);
        return;
    }
    plane.indexes.destroy();
    plane.status.destroy();
    plane.staging?.destroy();
})

/* Read a stale plane back once, checking the deferred palette errors and
 * invalid-index words that the skipped palette readback would have. */
EM_ASYNC_JS(int, webgpu_plane_sync, (int id, unsigned char *host, int size), {
    const state = Module['sixelWebGPU'];
    const plane = state?.planes?.map.get(id);
    if (!plane) return 0;
    if (!plane.stale) return 1;
    const device = state.device;
    let mapped = false;
    try {
        const length = plane.indexes.size, words = plane.status.size;
        if (!plane.staging)
            plane.staging = device.createBuffer({size:length + words,
                usage:GPUBufferUsage.MAP_READ | GPUBufferUsage.COPY_DST});
        const encoder = device.createCommandEncoder();
        encoder.copyBufferToBuffer(plane.indexes, 0, plane.staging, 0, length);
        encoder.copyBufferToBuffer(plane.status, 0, plane.staging, length,
            words);
        device.queue.submit([encoder.finish()]);
        const [errors] = await Promise.all([Promise.all(plane.pending),
            plane.staging.mapAsync(GPUMapMode.READ)]);
        mapped = true;
        if (errors.some(Boolean)) return 0;
        plane.pending = [];
        const view = plane.staging.getMappedRange();
        if (new Uint32Array(view, length, words / 4).some(Boolean)) return 0;
        HEAPU8.set(new Uint8Array(view, 0, size), host >>> 0);
        plane.stale = false;
        return 1;
    } catch (error) {
        return 0;
    } finally {
        if (mapped) plane.staging.unmap();
    }
})

typedef struct webgpu_index_plane {
    sixel_gpu_buffer_interface_t iface;
    unsigned int refs;
    int id;
    size_t size;
} webgpu_index_plane_t;

static void
webgpu_plane_ref(sixel_gpu_buffer_interface_t *self)
{
    ++((webgpu_index_plane_t *)self)->refs;
}

static void
webgpu_plane_unref(sixel_gpu_buffer_interface_t *self)
{
    webgpu_index_plane_t *plane;

    plane = (webgpu_index_plane_t *)self;
    if (plane == NULL || --plane->refs != 0) return;
    webgpu_plane_release(plane->id);
    free(plane);
}

static size_t
webgpu_plane_size(sixel_gpu_buffer_interface_t const *self)
{
    return ((webgpu_index_plane_t const *)self)->size;
}

/* WebGPU never maps storage into the Wasm heap. */
static SIXELSTATUS
webgpu_plane_shared_memory(sixel_gpu_buffer_interface_t const *self,
    void **memory)
{
    (void)self;
    if (memory != NULL) *memory = NULL;
    return SIXEL_FEATURE_ERROR;
}

static sixel_gpu_buffer_vtbl_t const webgpu_plane_vtbl = {
    webgpu_plane_ref, webgpu_plane_unref, webgpu_plane_size,
    webgpu_plane_shared_memory
};

SIXELSTATUS
sixel_webgpu_index_plane_new(size_t pixels,
    sixel_gpu_buffer_interface_t **result)
{
    webgpu_index_plane_t *plane;

    if (result == NULL) return SIXEL_BAD_ARGUMENT;
    *result = NULL;
    if (pixels == 0 || pixels > INT_MAX - 3) return SIXEL_BAD_ARGUMENT;
    if (!sixel_webgpu_available()) return SIXEL_FEATURE_ERROR;
    plane = malloc(sizeof(*plane));
    if (plane == NULL) return SIXEL_BAD_ALLOCATION;
    plane->id = webgpu_plane_new((int)pixels);
    if (plane->id == 0) {
        free(plane);
        return SIXEL_FEATURE_ERROR;
    }
    plane->iface.vtbl = &webgpu_plane_vtbl;
    plane->refs = 1;
    plane->size = pixels;
    *result = &plane->iface;
    return SIXEL_OK;
}

int
sixel_webgpu_index_plane_id(sixel_gpu_buffer_interface_t const *buffer)
{
    return buffer != NULL && buffer->vtbl == &webgpu_plane_vtbl
        ? ((webgpu_index_plane_t const *)buffer)->id : 0;
}

SIXELSTATUS
sixel_webgpu_index_plane_sync(sixel_gpu_buffer_interface_t *buffer,
    sixel_index_t *host)
{
    webgpu_index_plane_t *plane;

    if (sixel_webgpu_index_plane_id(buffer) == 0 || host == NULL)
        return SIXEL_BAD_ARGUMENT;
    plane = (webgpu_index_plane_t *)buffer;
    if (webgpu_plane_sync(plane->id, host, (int)plane->size)) return SIXEL_OK;
    sixel_helper_set_additional_message(
        "WebGPU palette dispatch or readback failed.");
    return SIXEL_FEATURE_ERROR;
}
#endif

/* A nonzero plane receives this chunk's indexes and invalid-index word on
 * the device. Nothing is read back; the plane keeps the error scopes. */
EM_ASYNC_JS(int, webgpu_dispatch, (char const *const *lookup, int nlookup,
    char const *const *dither, int ndither, unsigned char const *rgb,
    unsigned char const *palette, int colors, unsigned char *dest,
    unsigned int count, webgpu_params_t const *uniforms, uint32_t *corrected,
    unsigned char const *tile, unsigned char const *gradient,
    unsigned int gradient_size, void const *constants, unsigned int nbytes,
    unsigned int ws, sixel_gpu_lookup_build_pass_t const *passes,
    unsigned int npasses,
    char const *const *entry, int nentry, int batch, int plane), {
    let buffers = [], scoped = false, table;
    const state = Module['sixelWebGPU'];
    if (!state) return 0;
    const device = state.device;
    const target = plane ? state.planes?.map.get(plane) : null;
    if (plane && !target) return 0;
    try {
        const pipeline = await state.getPipeline(lookup, nlookup,
                                               dither, ndither, entry, nentry);
        table = await state.table(lookup, nlookup, palette, colors,
            constants, nbytes, ws, passes, npasses);
        if (!count) return 1;
        device.pushErrorScope('out-of-memory');
        device.pushErrorScope('validation');
        scoped = true;
        const U = GPUBufferUsage;
        const length = Math.ceil(count / 4) * 4;
        const noiseBytes = tile ? 4096 + Math.ceil(gradient_size / 4) * 4 : 0;
        state.paletteBuffers(buffers, count, colors, corrected, noiseBytes,
                             nbytes);
        const [input, pal, output, params, invalidIndex, queries, staging] =
            buffers;
        state.writeBytes(input, 0, rgb, 3 * count);
        const packed = new Uint32Array(colors);
        for (let i = 0; i < colors; ++i) {
            const p = (palette >>> 0) + i * 3;
            packed[i] = HEAPU8[p] | HEAPU8[p + 1] << 8 | HEAPU8[p + 2] << 16;
        }
        device.queue.writeBuffer(pal, 0, packed);
        state.writeBytes(params, 0, uniforms, 48);
        const entries = [input, pal, output, params, invalidIndex, queries]
            .map((b, binding) => ({binding, resource: {buffer: b}}));
        if (table) entries.push({binding:8,resource:{buffer:table.buffer}});
        if (tile) {
            const noise = buffers[7];
            state.writeBytes(noise, 0, tile, 4096);
            state.writeBytes(noise, 4096, gradient, gradient_size);
            entries.push({binding: 6, resource: {buffer: noise}});
        }
        if (nbytes) {
            const b = buffers[tile ? 8 : 7];
            state.writeBytes(b, 0, constants, nbytes);
            entries.push({binding: 7, resource: {buffer: b}});
        }
        const group = device.createBindGroup({
            layout: pipeline.getBindGroupLayout(0), entries
        });
        const enc = device.createCommandEncoder();
        enc.clearBuffer(invalidIndex);
        if (batch) enc.clearBuffer(output);
        const pass = enc.beginComputePass();
        pass.setPipeline(pipeline);
        pass.setBindGroup(0, group);
        if (batch) {
            const first = HEAPU32[(uniforms >>> 2) + 1];
            const width = HEAPU32[(uniforms >>> 2) + 2];
            const rows = Math.ceil(((first % width) + count) / width);
            const tiles = Math.ceil(width / 8) * Math.ceil(rows / 8);
            const limit = device.limits.maxComputeWorkgroupsPerDimension;
            const pitch = Math.min(tiles, limit);
            pass.dispatchWorkgroups(pitch,Math.ceil(tiles / pitch));
        } else pass.dispatchWorkgroups(Math.ceil(count / 256));
        pass.end();
        if (target) {
            // Chunks start on multiples of 1048576 pixels (see the caller).
            const first = HEAPU32[(uniforms >>> 2) + 1];
            enc.copyBufferToBuffer(output, 0, target.indexes, first, length);
            enc.copyBufferToBuffer(invalidIndex, 0, target.status,
                4 * Math.floor(first / 1048576), 4);
        } else {
            enc.copyBufferToBuffer(output, 0, staging, 0, length);
            enc.copyBufferToBuffer(invalidIndex, 0, staging, length, 4);
            if (corrected)
                enc.copyBufferToBuffer(queries, 0, staging, length + 4,
                    count * 4);
        }
        device.queue.submit([enc.finish()]);
        const validation = device.popErrorScope();
        const allocation = device.popErrorScope();
        scoped = false;
        if (target) {
            // Later queue work is ordered after these copies, so the next
            // chunk may reuse the leased buffers without waiting.
            target.pending.push(validation, allocation);
            target.device = target.stale = true;
            if (!state.idle) {
                state.idle = {count, colors, buffers, noiseBytes, nbytes,
                              saveQuery: false};
                buffers = [];
            }
            return 1;
        }
        const [invalid, exhausted] = await Promise.all([validation,
            allocation, staging.mapAsync(GPUMapMode.READ)]);
        if (invalid || exhausted) return 0;
        const mapped = staging.getMappedRange();
        if (new Uint32Array(mapped, length, 1)[0]) return 0;
        HEAPU8.set(new Uint8Array(mapped, 0, count),
                   dest >>> 0);
        if (corrected)
            HEAPU32.set(new Uint32Array(mapped, length + 4, count),
                        corrected >>> 2);
        staging.unmap();
        if (!state.idle) {
            state.idle = {count, colors, buffers, noiseBytes, nbytes,
                          saveQuery: !!corrected};
            buffers = [];
        }
        return 1;
    } catch (error) {
        return 0;
    } finally {
        if (scoped) {
            await device.popErrorScope();
            await device.popErrorScope();
        }
        table?.release();
        for (const b of buffers) b.destroy();
    }
})

static void
webgpu_device_ref(sixel_gpu_device_interface_t *self)
{
    (void)self;
}

static char const *
webgpu_format(sixel_gpu_device_interface_t const *self)
{
    (void)self;
    return "webgpu.lookup-rgb8.v1";
}

static SIXELSTATUS
webgpu_buffer(sixel_gpu_device_interface_t *self, size_t size,
    sixel_gpu_buffer_interface_t **result)
{
    (void)self;
    (void)size;
    if (result == NULL) return SIXEL_BAD_ARGUMENT;
    *result = NULL;
    return SIXEL_FEATURE_ERROR;
}

static sixel_gpu_device_vtbl_t const webgpu_vtbl = {
    webgpu_device_ref, webgpu_device_ref, webgpu_format, webgpu_buffer
};
static sixel_gpu_device_interface_t webgpu_device = { &webgpu_vtbl };

SIXELSTATUS
sixel_gpu_device_acquire(sixel_gpu_device_interface_t **result)
{
    if (result == NULL) return SIXEL_BAD_ARGUMENT;
    *result = sixel_webgpu_available() ? &webgpu_device : NULL;
    return *result != NULL ? SIXEL_OK : SIXEL_FEATURE_ERROR;
}

/* All WebGPU consumers use the same lookup ABI. The provider supplies
 * either point queries or a collective over two queries per lane of an
 * 8 x 8 tile. Builders bind packed RGB at 1, constants at 7 and scratch
 * at 8. Only the provider knows the contents of that scratch. */
SIXELSTATUS
sixel_webgpu_check_lookup(sixel_gpu_lookup_program_view_t const *view,
    sixel_gpu_lookup_query_kind_t kind)
{
    sixel_gpu_lookup_batch_dispatch_t const *batch;
    size_t i;

    batch = view->batch;
    if (view->program_format == NULL ||
            strcmp(view->program_format, "webgpu.lookup-rgb8.v1") ||
            view->query_kind != kind ||
            (batch && (batch->entry_point == NULL ||
                strcmp(batch->entry_point, "lookup_pair") ||
                batch->tile[0] != 8 || batch->tile[1] != 8 ||
                batch->threads[0] != 32 || batch->threads[1] != 1 ||
                batch->threads[2] != 1)))
        return SIXEL_FEATURE_ERROR;
    if (view->palette == NULL || view->ncolors < 1 || view->ncolors > 256 ||
            view->palette_size < 3U * view->ncolors || !view->chunks ||
            !view->chunk_count || view->chunk_count > INT_MAX ||
            view->constants_size % 4U ||
            (view->constants_size && !view->constants) ||
            (view->workspace_size == 0) != (view->build_pass_count == 0) ||
            (view->build_pass_count && !view->build_passes))
        return SIXEL_BAD_ARGUMENT;
    for (i = 0; i < view->chunk_count; ++i)
        if (view->chunks[i] == NULL) return SIXEL_BAD_ARGUMENT;
    for (i = 0; i < view->build_pass_count; ++i)
        if (view->build_passes[i].entry_point == NULL)
            return SIXEL_BAD_ARGUMENT;
    return SIXEL_OK;
}

SIXELSTATUS
sixel_gpu_program_prepare(sixel_gpu_device_interface_t *device,
    sixel_gpu_lookup_plan_interface_t *lookup,
    sixel_gpu_dither_program_t const *dither, sixel_gpu_program_t **result)
{
    sixel_gpu_lookup_program_view_t view;
    sixel_gpu_program_t *program;
    SIXELSTATUS status;

    if (result == NULL) return SIXEL_BAD_ARGUMENT;
    *result = NULL;
    if (dither == NULL || lookup == NULL) return SIXEL_BAD_ARGUMENT;
    if (device != &webgpu_device || dither->suffix == NULL ||
            !dither->suffix_count || !dither->suffix[0])
        return SIXEL_FEATURE_ERROR;
    status = lookup->vtbl->describe(lookup, &view);
    if (status != SIXEL_OK) return status;
    status = sixel_webgpu_check_lookup(&view, SIXEL_GPU_LOOKUP_QUERY_BYTE);
    if (status != SIXEL_OK) return status;
    /* Probe before selecting the plan so AUTO can still choose the CPU if
     * the browser declines the adapter or cannot compile this program. */
    if (!sixel_webgpu_available() ||
            !webgpu_dispatch(view.chunks, (int)view.chunk_count,
                dither->suffix, (int)dither->suffix_count, NULL, view.palette,
                view.ncolors, NULL, 0, NULL, NULL, NULL, NULL, 0,
                view.constants, view.constants_size, view.workspace_size,
                view.build_passes, view.build_pass_count,
                view.batch ? palette_batch_main : palette_point_main,
                view.batch ? sizeof(palette_batch_main) / sizeof(char *) :
                    sizeof(palette_point_main) / sizeof(char *),
                view.batch != NULL, 0)) {
        sixel_helper_set_additional_message(
            "WebGPU adapter or compute pipeline unavailable.");
        return SIXEL_FEATURE_ERROR;
    }
    program = malloc(sizeof(*program));
    if (program == NULL) return SIXEL_BAD_ALLOCATION;
    sixel_gpu_statistics_set_device("webgpu", "WebGPU adapter");
    program->view = view;
    program->dither = dither;
    program->lookup = lookup;
    lookup->vtbl->ref(lookup);
    *result = program;
    return SIXEL_OK;
}

void
sixel_gpu_program_destroy(sixel_gpu_program_t *program)
{
    if (program != NULL) program->lookup->vtbl->unref(program->lookup);
    free(program);
}

static int
webgpu_distance(unsigned char const *a, unsigned char const *b)
{
    int r, g, blue;

    r = a[0] - b[0];
    g = a[1] - b[1];
    blue = a[2] - b[2];
    return r * r + g * g + blue * blue;
}

/* Retained history already lives in Wasm. Apply the native early and late
 * keep decisions there, including emitted-palette rounding and mask flags,
 * without uploading a full retained canvas for a small damage rectangle. */
static unsigned char
webgpu_keep(sixel_gpu_palette_request_t const *r, size_t i,
    uint32_t const *corrected)
{
    size_t retained;
    unsigned char const *source, *prior, *chosen;
    unsigned int threshold;
    unsigned char query[3];
    int x, y, stride, within;

    x = (int)(i % (size_t)r->width);
    y = (int)(i / (size_t)r->width);
    stride = r->accumulation_width ? r->accumulation_width : r->width;
    retained = (size_t)(r->accumulation_origin_y + y) * (size_t)stride +
        (size_t)(r->accumulation_origin_x + x);
    if (r->accumulation_valid_mask && !r->accumulation_valid_mask[retained])
        return 0;
    source = r->pixels + i * 3U;
    if (corrected != NULL) {
        query[0] = (unsigned char)*corrected;
        query[1] = (unsigned char)(*corrected >> 8);
        query[2] = (unsigned char)(*corrected >> 16);
        source = query;
    }
    prior = r->accumulation_pixels + retained * 3U;
    threshold = sixel_delta_mask_threshold_rgb888(r->residual_mask,
        x, y, r->sixdelta_threshold, source, prior);
    within = r->sixdelta_metric == SIXEL_DELTA_METRIC_RGB241
        ? sixel_delta_rgb241_within(source, prior, threshold)
        : (unsigned int)abs(source[0] - prior[0]) <= threshold &&
          (unsigned int)abs(source[1] - prior[1]) <= threshold &&
          (unsigned int)abs(source[2] - prior[2]) <= threshold;
    if (within) return memcmp(source, prior, 3) == 0 ? 1 : 3;
    chosen = r->sixdelta_comparison_palette != NULL
        ? r->sixdelta_comparison_palette : r->palette;
    return webgpu_distance(source, prior) <=
        webgpu_distance(source, chosen + r->dest[i] * 3U);
}

SIXELSTATUS
sixel_webgpu_apply(sixel_gpu_palette_request_t const *r)
{
    sixel_gpu_program_t *program;
    sixel_gpu_lookup_binding_interface_t *binding;
    webgpu_params_t params;
    size_t first, count, i, gradient_size;
    unsigned char const *gradient;
    uint32_t *corrected;
    unsigned char skip;
    int transparent, plane;
    SIXELSTATUS status, finish;

    program = r->program;
    if (program == NULL || program->dither != r->dither_program ||
            (unsigned int)r->ncolors != program->view.ncolors ||
            memcmp(r->palette, program->view.palette,
                3U * (size_t)r->ncolors) != 0)
        return SIXEL_BAD_ARGUMENT;
    if (r->accumulation_result_mask != NULL &&
            r->accumulation_result_mask_size < r->pixel_count)
        return SIXEL_BAD_ARGUMENT;
    if (!sixel_webgpu_available()) return SIXEL_FEATURE_ERROR;
    binding = NULL;
    status = program->lookup->vtbl->begin(program->lookup, &binding);
    if (status != SIXEL_OK) return status;
    transparent = r->transparent_mask != NULL &&
        r->transparent_keycolor >= 0 && r->transparent_keycolor < 256;
    /* Keep the indexes on the device only when no host pass below or a
     * row subscriber reads them before the frame's consumer syncs. */
    plane = 0;
#if !SIXEL_ENABLE_THREADS
    if (!transparent && !r->has_6delta_accumulation && r->rows_ready == NULL &&
            r->dest_buffer != NULL &&
            r->dest_buffer->vtbl->size(r->dest_buffer) == r->pixel_count)
        plane = sixel_webgpu_index_plane_id(r->dest_buffer);
#endif
    corrected = NULL;
    if (r->has_6delta_accumulation &&
            r->sixdelta_compare == SIXEL_DELTA_COMPARE_QUERY) {
        count = r->pixel_count < 1048576U ? r->pixel_count : 1048576U;
        corrected = malloc(sizeof(*corrected) * count);
        if (corrected == NULL) status = SIXEL_BAD_ALLOCATION;
    }
    memset(&params, 0, sizeof(params));
    params.width = (uint32_t)r->width;
    params.strength = r->bluenoise_tile != NULL
        ? r->bluenoise_strength : r->positional_strength;
    params.save_query = corrected != NULL;
    params.phase_x = (uint32_t)r->bluenoise_phase_x;
    params.phase_y = (uint32_t)r->bluenoise_phase_y;
    params.channel_rgb = r->bluenoise_channel_rgb != 0;
    params.gradient_factor = r->bluenoise_gradient_factor;
    params.gradient_width = (uint32_t)r->bluenoise_gradient_width;
    params.gradient_height = (uint32_t)r->bluenoise_gradient_height;
    /* At most 3 MiB per upload and 4096 workgroups. Even the minimum
     * WebGPU limits suffice, regardless of the full frame dimensions. */
    for (first = 0; status == SIXEL_OK && first < r->pixel_count;
            first += count) {
        if (r->cancel != NULL && r->cancel(r->cancel_context)) {
            status = SIXEL_INTERRUPTED;
            break;
        }
        count = r->pixel_count - first;
        if (count > 1048576U) count = 1048576U;
        gradient_size = 0;
        gradient = NULL;
        if (r->bluenoise_tile && r->bluenoise_gradient_map &&
                first < r->bluenoise_gradient_map_size) {
            gradient = r->bluenoise_gradient_map + first;
            gradient_size = r->bluenoise_gradient_map_size - first;
            if (gradient_size > count) gradient_size = count;
        }
        params.gradient_size = (uint32_t)gradient_size;
        params.first = (uint32_t)first;
        params.count = (uint32_t)count;
        if (!webgpu_dispatch(program->view.chunks,
                (int)program->view.chunk_count, program->dither->suffix,
                (int)program->dither->suffix_count, r->pixels + first * 3U,
                r->palette, r->ncolors, r->dest + first,
                (unsigned int)count, &params, corrected, r->bluenoise_tile,
                gradient, (unsigned int)gradient_size, program->view.constants,
                (unsigned int)program->view.constants_size,
                program->view.workspace_size, program->view.build_passes,
                program->view.build_pass_count,
                program->view.batch ? palette_batch_main : palette_point_main,
                program->view.batch ?
                    sizeof(palette_batch_main) / sizeof(char *) :
                    sizeof(palette_point_main) / sizeof(char *),
                program->view.batch != NULL, plane)) {
            sixel_helper_set_additional_message(
                "WebGPU palette dispatch or readback failed.");
            status = SIXEL_FEATURE_ERROR;
            break;
        }
        /* No timestamp query is required by this backend. Record completion
         * without presenting host wait time as device busy time. */
        sixel_gpu_statistics_record(0);
        if (!transparent && !r->has_6delta_accumulation) {
            if (r->accumulation_result_mask != NULL)
                memset(r->accumulation_result_mask + first, 0, count);
            continue;
        }
        for (i = first; i < first + count; ++i) {
            skip = 0;
            if (transparent && r->transparent_mask[i]) {
                r->dest[i] = (unsigned char)r->transparent_keycolor;
                skip = 1;
            } else if (r->has_6delta_accumulation) {
                skip = webgpu_keep(r, i,
                    corrected != NULL ? corrected + (i - first) : NULL);
                if (skip) r->dest[i] = (unsigned char)r->accumulation_keycolor;
            }
            if (r->accumulation_result_mask != NULL)
                r->accumulation_result_mask[i] = skip;
        }
    }
    if (status == SIXEL_OK && r->cancel && r->cancel(r->cancel_context))
        status = SIXEL_INTERRUPTED;
    if (status == SIXEL_OK && r->rows_per_chunk > 0 && r->rows_ready != NULL)
        status = r->rows_ready(r->rows_context, r->height);
    free(corrected);
    finish = binding->vtbl->finish(binding, status);
    binding->vtbl->unref(binding);
    return status == SIXEL_OK ? finish : status;
}
#endif /* HAVE_WEBGPU */
