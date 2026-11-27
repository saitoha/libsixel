/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if defined(HAVE_WEBGPU) && !SIXEL_ENABLE_THREADS
#include <emscripten.h>

#include "encoder-body-gpu.h"
#include "gpu-palette.h"
#include "gpu-statistics.h"

static char const *const webgpu_body_shader[] = {
#include <encoder-body-webgpu-wgsl.inc.h>
};

/* Pipeline layouts are local to each stage so no stage needs more than
 * eight storage buffers. Cache the working set for consecutive frames. */
EM_JS(void, sixel_webgpu_body_install,
      (char const *const *chunks, int count), {
    const state = Module['sixelWebGPU'];
    if (state.body) return;
    const device = state.device;
    const U = GPUBufferUsage;
    const stages = [
        ['classify', [0,1,2,5,6,13]],
        ['count_spans', [0,2,3,4,11]],
        ['span_offsets', [0,3,11]],
        ['span_bases', [0,11]],
        ['write_spans', [0,2,3,4,11]],
        ['span_lengths', [0,4,5,7,11]],
        ['span_order', [0,4,6,7,8,9,10,11,14]],
        ['span_order_group', [0,4,6,7,9,10,11,14]],
        ['byte_offsets', [0,4,10,11,12,14]],
        ['span_write', [0,4,5,10,11,12,14]]
    ];
    const program = state.chunks(chunks, count);
    state.body = {busy:false, cache:null, pipelines:null};
    state.body.prepare = async sizes => {
        const body = state.body;
        const old = body.cache;
        if (old && sizes.every((v, i) => old.sizes[i] === v)) return old;
        if (sizes.some((v, i) => v > device.limits.maxBufferSize ||
                (i > 0 && i < 15 &&
                 v > device.limits.maxStorageBufferBindingSize))) return null;
        if (!body.pipelines) {
            const module = device.createShaderModule({code:program});
            body.pipelines = await Promise.all(stages.map(([entryPoint]) =>
                device.createComputePipelineAsync({layout:'auto',
                    compute:{module, entryPoint}})));
        }
        const usage = [
            U.UNIFORM|U.COPY_DST, U.STORAGE|U.COPY_DST, U.STORAGE|U.COPY_DST,
            U.STORAGE, U.STORAGE, U.STORAGE|U.COPY_DST, U.STORAGE|U.COPY_DST,
            U.STORAGE, U.STORAGE, U.STORAGE, U.STORAGE, U.STORAGE|U.COPY_SRC,
            U.STORAGE|U.COPY_DST|U.COPY_SRC, U.STORAGE|U.COPY_DST,
            U.STORAGE|U.COPY_DST, U.MAP_READ|U.COPY_DST, U.MAP_READ|U.COPY_DST
        ];
        const buffers = [];
        device.pushErrorScope('out-of-memory');
        device.pushErrorScope('validation');
        let groups;
        try {
            sizes.forEach((size,i) => buffers.push(
                device.createBuffer({size, usage:usage[i]})));
            groups = body.pipelines.map((pipeline, i) =>
                device.createBindGroup({
                    layout:pipeline.getBindGroupLayout(0),
                    entries:stages[i][1].map(binding =>
                        ({binding, resource:{buffer:buffers[binding]}}))
                }));
        } finally {
            const errors = await Promise.all([
                device.popErrorScope(), device.popErrorScope()
            ]);
            if (!groups || errors.some(Boolean)) {
                buffers.forEach(b => b.destroy());
                groups = null;
            }
        }
        if (!groups) return null;
        old?.buffers.forEach(b => b.destroy());
        return body.cache = {sizes,buffers,groups};
    };
})

EM_JS_DEPS(sixel_webgpu_body_memory, "malloc,free")

/* Output length stays on the device until spelling finishes. Read that
 * word first and copy only the aligned range occupied by the body. A
 * resident index plane is copied on the device; its deferred palette
 * errors and invalid-index words are checked with the length. */
EM_ASYNC_JS(unsigned char *, sixel_webgpu_body_dispatch,
      (unsigned char const *indexes, unsigned char const *mask,
       int width, int height, int colors, int keycolor, int selectors,
       int limited, uint32_t const *registers, uint32_t *length,
       double *elapsed, int plane), {
    const state = Module['sixelWebGPU'];
    const {device, body} = state;
    if (body.busy) return 0;
    const source = plane ? state.planes?.map.get(plane) : null;
    const resident = !!source?.device;
    body.busy = true;
    let result = 0;
    let sizeBuffer, readback;
    let lengthMapped = false, bodyMapped = false, scoped = false;
    try {
        const pixels = width * height;
        const bands = Math.ceil(height / 6);
        const words = Math.ceil(width / 32);
        const pairs = bands * colors;
        const nodes = Math.min(pixels, pairs * Math.ceil(width / 11));
        const cells = Math.min(10 * pixels, pairs * width);
        const gap = (limited ? 5 * Math.floor(width / 255) : 0)
            + 2 + String(width).length;
        const register = registers >>> 2;
        const regs = HEAPU32.slice(register, register + colors);
        const selector = 1 + String(Math.max(...regs)).length;
        const align = n => 4 * Math.ceil(n / 4);
        const bound = align(cells + nodes * (1 + selector + gap) + bands);
        // The length word is followed by one status word per palette
        // chunk of a resident plane.
        const chunks = Math.ceil(pixels / 1048576);
        const sizes = [48, align(pixels), pairs * words * 4, pairs * 8,
            nodes * 16, align(pairs * width), pairs * 4, nodes * 4,
            bands * (2 * width + 1 + words) * 4, nodes * 4, nodes * 16,
            (bands + 1) * 16, bound, mask ? align(pixels) : 4, colors * 4,
            4 + 4 * chunks, bound];
        const wg = n => Math.ceil(n / 256);
        const counts = [wg(pixels),wg(pairs),bands,1,wg(pairs),
                        wg(nodes),bands,bands,1,wg(nodes)];
        const limit = device.limits.maxComputeWorkgroupsPerDimension;
        if (counts.some(n => n > limit))
            return 0;
        const cache = await body.prepare(sizes);
        if (!cache) return 0;
        const start = performance.now();
        const b = cache.buffers;
        sizeBuffer = b[15];
        readback = b[16];
        device.pushErrorScope('validation');
        scoped = true;
        device.queue.writeBuffer(b[0], 0, new Uint32Array([width,height,colors,
            bands,words,pairs,limited,selectors,keycolor,Number(!!mask),0,0]));
        if (!resident) state.writeBytes(b[1], 0, indexes, pixels);
        if (mask) state.writeBytes(b[13], 0, mask, pixels);
        device.queue.writeBuffer(b[14], 0, regs);
        const encoder = device.createCommandEncoder();
        if (resident)
            encoder.copyBufferToBuffer(source.indexes, 0, b[1], 0,
                align(pixels));
        for (const i of [2,5,6,12]) encoder.clearBuffer(b[i]);
        body.pipelines.forEach((pipeline, i) => {
            const pass = encoder.beginComputePass();
            pass.setPipeline(pipeline);
            pass.setBindGroup(0, cache.groups[i]);
            pass.dispatchWorkgroups(counts[i]);
            pass.end();
        });
        encoder.copyBufferToBuffer(b[11], 16 * bands + 8, sizeBuffer, 0, 4);
        if (resident)
            encoder.copyBufferToBuffer(source.status, 0, sizeBuffer, 4,
                4 * chunks);
        device.queue.submit([encoder.finish()]);
        // Wait for the palette's error scopes with the length, not before
        // submitting, so the body queues behind the palette on the device.
        const [errors] = await Promise.all([Promise.all(resident ?
            source.pending : []), sizeBuffer.mapAsync(GPUMapMode.READ)]);
        lengthMapped = true;
        if (errors.some(Boolean)) return 0;
        if (resident) source.pending = [];
        const status = new Uint32Array(sizeBuffer.getMappedRange());
        const used = status[0];
        if (!used || used > bound) return 0;
        if (resident && status.subarray(1, 1 + chunks).some(Boolean))
            return 0;
        const copy = device.createCommandEncoder();
        copy.copyBufferToBuffer(b[12], 0, readback, 0, align(used));
        device.queue.submit([copy.finish()]);
        await readback.mapAsync(GPUMapMode.READ, 0, align(used));
        bodyMapped = true;
        const invalid = device.popErrorScope();
        scoped = false;
        if (await invalid) return 0;
        result = _malloc(used);
        if (!result) return 0;
        HEAPU8.set(new Uint8Array(
            readback.getMappedRange(0, align(used)), 0, used), result >>> 0);
        HEAPU32[length >>> 2] = used;
        HEAPF64[elapsed >>> 3] = performance.now() - start;
        return result;
    } catch (error) {
        if (result) _free(result);
        return 0;
    } finally {
        if (bodyMapped) readback.unmap();
        if (lengthMapped) sizeBuffer.unmap();
        if (scoped) await device.popErrorScope();
        body.busy = false;
    }
})

unsigned char *
sixel_body_webgpu_spell(sixel_body_gpu_request_t const *request,
                       uint32_t *length, double *elapsed)
{
    unsigned char *bytes;

    if (!sixel_webgpu_available()) return NULL;
    sixel_webgpu_body_install(webgpu_body_shader,
        sizeof(webgpu_body_shader) / sizeof(webgpu_body_shader[0]));
    bytes = sixel_webgpu_body_dispatch(request->indexes, request->mask,
        request->width, request->height, request->ncolors, request->keycolor,
        request->selectors, request->limited, request->registers,
        length, elapsed, sixel_webgpu_index_plane_id(request->indexes_buffer));
    if (bytes != NULL) {
        /* Count the compute and byte-copy submissions without charging
         * host readback time as device busy time. */
        sixel_gpu_statistics_set_device("webgpu", "WebGPU adapter");
        sixel_gpu_statistics_record(0);
        sixel_gpu_statistics_record(0);
    }
    return bytes;
}
#endif
