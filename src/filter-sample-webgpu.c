/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if defined(HAVE_WEBGPU)
#include <limits.h>
#include <stdint.h>
#include <emscripten.h>

#include "filter-sample.h"
#include "gpu-palette.h"
#include "gpu-statistics.h"

static char const *const sample_solid_wgsl[] = {
#include <filter-sample-gpu-wgsl.inc.h>
};

typedef struct sixel_webgpu_sample_params {
    int32_t width, height, clip_x, clip_y;
    uint32_t points, rows;
    int32_t tolerance;
    uint32_t stride;
} sixel_webgpu_sample_params_t;

/* The frame upload and histogram are private to this scanner. Keep one
 * lease per JS realm so a concurrent sample can fall back without sharing
 * in-flight buffers or holding a Wasm heap view across an await. */
EM_ASYNC_JS(int, sixel_webgpu_sample_scan,
    (char const *const *chunks, int chunk_count,
     unsigned char const *pixels, unsigned int bytes,
     sixel_webgpu_sample_params_t const *params,
     unsigned int *cells, unsigned int histogram_bytes), {
    const state = Module['sixelWebGPU'];
    if (!state) return 0;
    const device = state.device;
    const scan = state.sampleScan ?? (state.sampleScan = {busy:false});
    const size = Math.ceil(bytes / 4) * 4;
    const limit = device.limits;
    const word = params >>> 2;
    const gx = Math.ceil(HEAPU32[word + 4] / 32);
    const gy = Math.ceil(HEAPU32[word + 5] / 4);
    if (scan.busy || size > limit.maxBufferSize ||
            size > limit.maxStorageBufferBindingSize ||
            gx > limit.maxComputeWorkgroupsPerDimension ||
            gy > limit.maxComputeWorkgroupsPerDimension) return 0;
    scan.busy = true;
    let staging, mapped = false, scoped = false, pending = [];
    try {
        device.pushErrorScope('out-of-memory');
        device.pushErrorScope('validation');
        scoped = true;
        if (!scan.pipeline) {
            const module = device.createShaderModule({
                code:state.chunks(chunks, chunk_count)
            });
            scan.pipeline = await device.createComputePipelineAsync({
                layout:'auto', compute:{module, entryPoint:'sample_solid'}
            });
        }
        let cache = scan.cache;
        const replace = !cache || cache.size < size;
        if (replace) {
            const U = GPUBufferUsage;
            const buffer = (size, usage) => {
                const b = device.createBuffer({size, usage});
                pending.push(b);
                return b;
            };
            const source = buffer(size, U.STORAGE | U.COPY_DST);
            const histogram = buffer(histogram_bytes,
                U.STORAGE | U.COPY_SRC | U.COPY_DST);
            const constants = buffer(32, U.UNIFORM | U.COPY_DST);
            staging = buffer(histogram_bytes, U.MAP_READ | U.COPY_DST);
            const group = device.createBindGroup({
                layout:scan.pipeline.getBindGroupLayout(0),
                entries:[source,histogram,constants].map((buffer, binding) =>
                    ({binding, resource:{buffer}}))
            });
            cache = {size,source,histogram,constants,staging,group,
                buffers:pending};
        }
        staging = cache.staging;
        state.writeBytes(cache.source, 0, pixels, bytes);
        state.writeBytes(cache.constants, 0, params, 32);
        const encoder = device.createCommandEncoder();
        encoder.clearBuffer(cache.histogram);
        const pass = encoder.beginComputePass();
        pass.setPipeline(scan.pipeline);
        pass.setBindGroup(0, cache.group);
        pass.dispatchWorkgroups(gx, gy);
        pass.end();
        encoder.copyBufferToBuffer(cache.histogram, 0, staging, 0,
                                   histogram_bytes);
        device.queue.submit([encoder.finish()]);
        const invalid = device.popErrorScope();
        const exhausted = device.popErrorScope();
        scoped = false;
        const errors = await Promise.all([invalid, exhausted]);
        if (errors.some(Boolean)) return 0;
        await staging.mapAsync(GPUMapMode.READ);
        mapped = true;
        HEAPU8.set(new Uint8Array(staging.getMappedRange()), cells >>> 0);
        if (replace) {
            scan.cache?.buffers.forEach(b => b.destroy());
            scan.cache = cache;
            pending = [];
        }
        return 1;
    } catch (error) {
        return 0;
    } finally {
        if (mapped) staging.unmap();
        if (scoped) {
            const invalid = device.popErrorScope();
            const exhausted = device.popErrorScope();
            await Promise.all([invalid, exhausted]);
        }
        pending.forEach(b => b.destroy());
        scan.busy = false;
    }
})

SIXELSTATUS
sixel_filter_sample_solid_gpu(unsigned char const *pixels,
                              int width,
                              int height,
                              int clip_x,
                              int clip_y,
                              int clip_width,
                              int clip_height,
                              sixel_frame_t *share_owner,
                              unsigned int *cells)
{
    sixel_webgpu_sample_params_t params;
    unsigned int bytes;

    (void)share_owner;
    if (pixels == NULL || cells == NULL || width <= 0 || height <= 0 ||
            clip_width <= 0 || clip_height <= 0 ||
            clip_x < 0 || clip_y < 0 || clip_x > width - clip_width ||
            clip_y > height - clip_height ||
            (unsigned int)width > UINT_MAX / 3u / (unsigned int)height) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (!sixel_webgpu_available()) return SIXEL_FEATURE_ERROR;
    bytes = (unsigned int)width * (unsigned int)height * 3u;
    params.width = width;
    params.height = height;
    params.clip_x = clip_x;
    params.clip_y = clip_y;
    params.points = 1u + (unsigned int)(clip_width - 1)
        / SIXEL_SAMPLE_SOLID_STRIDE;
    params.rows = 1u + (unsigned int)(clip_height - 1)
        / SIXEL_SAMPLE_SOLID_STRIDE;
    params.tolerance = SIXEL_SAMPLE_SOLID_TOL;
    params.stride = SIXEL_SAMPLE_SOLID_STRIDE;
    if (!sixel_webgpu_sample_scan(sample_solid_wgsl,
            sizeof(sample_solid_wgsl) / sizeof(sample_solid_wgsl[0]),
            pixels, bytes, &params, cells,
            SIXEL_SAMPLE_SOLID_CELLS * 5u * sizeof(*cells))) {
        return SIXEL_FEATURE_ERROR;
    }
    sixel_gpu_statistics_set_device("webgpu", "WebGPU adapter");
    sixel_gpu_statistics_record(0);
    return SIXEL_OK;
}
#endif
