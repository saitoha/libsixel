/* SPDX-License-Identifier: MIT */

/* The parent's Metal mutex owns this single immutable table. Unlike the CPU
 * first-query memo, every cell has a deterministic representative. Publish
 * its identity only after the build and application command has completed. */
static sixel_gpu_metal_cached_buffer_t g_sixel_gpu_bitlut = { nil, 0U };
static id<MTLComputePipelineState> g_sixel_gpu_bitlut_build = nil;
static unsigned char g_sixel_gpu_bitlut_palette[SIXEL_PALETTE_MAX * 3];
static int g_sixel_gpu_bitlut_colors;
static int g_sixel_gpu_bitlut_bits;

static char const * const g_sixel_gpu_bitlut_source[] = {
"#include <metal_stdlib>\n"
"using namespace metal;\n"
"kernel void bitlut_build(device uchar *table [[buffer(0)]],\n"
"    constant uchar *palette [[buffer(1)]],\n"
"    constant uint2 &p [[buffer(2)]],\n"
"    uint cell [[thread_position_in_grid]])\n"
"{\n"
"    uint bits, side, shift, half_step, best, distance, i, d;\n"
"    uint3 address;\n"
"    int3 low, high, center2, delta;\n"
"    bits = p.x;\n"
"    side = 1u << bits;\n"
"    if (cell >= side * side * side) return;\n"
"    shift = 8u - bits;\n"
"    half_step = 1u << (shift - 1u);\n"
"    address = uint3(cell >> (2u * bits),\n"
"        (cell >> bits) & (side - 1u), cell & (side - 1u));\n"
"    low = max(int3(address << shift) - int(half_step), int3(0));\n"
"    high = int3((address + 1u) << shift) - int(half_step) - 1;\n"
"    high = select(high, int3(255), address == side - 1u);\n"
"    // Twice the center keeps half-byte endpoints exact, including\n"
"    // the clipped first cell and the saturated final cell.\n"
"    center2 = low + high;\n"
"    best = 0u;\n"
"    distance = UINT_MAX;\n"
"    for (i = 0u; i < p.y; ++i) {\n"
"        delta = center2 - 2 * int3(palette[3u * i],\n"
"            palette[3u * i + 1u], palette[3u * i + 2u]);\n"
"        d = uint(delta.x * delta.x + delta.y * delta.y +\n"
"            delta.z * delta.z);\n"
"        if (d < distance) { distance = d; best = i; }\n"
"    }\n"
"    table[cell] = uchar(best);\n"
"}\n",
NULL
};

static int
sixel_gpu_bitlut_prepare(void)
{
    NSMutableString *source;
    id<MTLLibrary> library;
    id<MTLFunction> function;
    NSError *error;

    if (g_sixel_gpu_bitlut_build != nil) return 1;
    source = [[NSMutableString alloc] init];
    if (source == nil) return 0;
    if (!sixel_gpu_palette_metal_append_chunks(source,
            g_sixel_gpu_bitlut_source)) {
        [source release];
        return 0;
    }
    error = nil;
    library = [g_sixel_gpu_metal_device newLibraryWithSource:source
        options:nil error:&error];
    [source release];
    if (library == nil) {
        sixel_gpu_palette_metal_set_message(
            "gpu bit lookup: Metal compilation failed", error);
        return 0;
    }
    function = [library newFunctionWithName:@"bitlut_build"];
    if (function != nil) {
        g_sixel_gpu_bitlut_build = [g_sixel_gpu_metal_device
            newComputePipelineStateWithFunction:function error:&error];
        [function release];
    }
    [library release];
    return g_sixel_gpu_bitlut_build != nil;
}

static SIXELSTATUS
sixel_gpu_bitlut_encode(id<MTLCommandBuffer> command,
    id<MTLBuffer> palette, id<MTLBuffer> table, unsigned int bits, int colors)
{
    id<MTLComputeCommandEncoder> encoder;
    uint32_t parameters[2];
    NSUInteger width, count;

    encoder = [command computeCommandEncoder];
    if (encoder == nil) return SIXEL_RUNTIME_ERROR;
    parameters[0] = bits;
    parameters[1] = (uint32_t)colors;
    count = (NSUInteger)1U << (bits * 3U);
    width = [g_sixel_gpu_bitlut_build maxTotalThreadsPerThreadgroup];
    if (width > 128U) width = 128U;
    if (width == 0U) width = 1U;
    [encoder setComputePipelineState:g_sixel_gpu_bitlut_build];
    [encoder setBuffer:table offset:0 atIndex:0];
    [encoder setBuffer:palette offset:0 atIndex:1];
    [encoder setBytes:parameters length:sizeof(parameters) atIndex:2];
    [encoder dispatchThreadgroups:MTLSizeMake((count + width - 1U) / width,
        1U, 1U) threadsPerThreadgroup:MTLSizeMake(width, 1U, 1U)];
    [encoder endEncoding];
    return SIXEL_OK;
}
