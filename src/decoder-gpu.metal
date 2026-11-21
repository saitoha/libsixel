/* SPDX-License-Identifier: MIT */
#include <metal_stdlib>
using namespace metal;

/* A chunk is a piece of the body between two '$' / '-' delimiters, cut
 * after a sixel byte so no token spans two chunks.  BAND counts the '-'
 * before it and SEG numbers its segment; one thread parses one chunk. */
struct DecoderGpuSegment {
    uint start, end, band, seg;
};

struct DecoderGpuParams {
    uint segments, width, height, bands, initial_color;
};

/* Flags: the CPU parser must decide; paint fell outside the raster; a pixel
 * below height stayed unpainted. */
constant uint DECODER_GPU_NEEDS_CPU = 1u;
constant uint DECODER_GPU_OUTSIDE = 2u;
constant uint DECODER_GPU_TRANSPARENT = 4u;

/* Digits from P, as the CPU workers read them; nonzero when the number
 * exceeds the 16-bit limit at which they give up. */
static uint
decoder_gpu_number(device const uchar *body, thread uint &p, uint end,
                   thread uint &value)
{
    uint d;

    value = 0u;
    while (p < end) {
        d = (uint)body[p] - 48u;
        if (d > 9u)
            break;
        value = value * 10u + d;
        ++p;
        if (value > 65535u)
            return 1u;
    }
    return 0u;
}

/* Pass one: the columns a chunk advances and the last selection it makes,
 * with bit 8 set when it makes one. */
/* Accel: DECODE-BAND-PARSE metal */
kernel void
decoder_gpu_measure(device const uchar *body [[buffer(0)]],
                    device const DecoderGpuSegment *segments [[buffer(1)]],
                    device uint2 *info [[buffer(2)]],
                    constant DecoderGpuParams &params [[buffer(3)]],
                    uint index [[thread_position_in_grid]])
{
    DecoderGpuSegment segment;
    ulong advance;
    uint color;
    uint repeat;
    uint value;
    uint p;
    uint ch;

    if (index >= params.segments)
        return;
    segment = segments[index];
    advance = 0ul;
    color = 0u;
    repeat = 1u;
    p = segment.start;
    while (p < segment.end) {
        ch = body[p];
        ++p;
        if (ch - 63u <= 63u) {
            advance += repeat;
            repeat = 1u;
        } else if (ch == '#' || ch == '!') {
            /* Overflow and wide indexes stop pass two, which reads the
             * same digits. */
            (void)decoder_gpu_number(body, p, segment.end, value);
            if (ch == '#')
                color = 0x100u | (value & 255u);
            else
                repeat = value > 0u ? value : 1u;
        }
    }
    info[index] = uint2((uint)min(advance, 0xfffffffful), color);
}

/* Accel: DECODE-BAND-PARSE metal */
kernel void
decoder_gpu_parse(device const uchar *body [[buffer(0)]],
                  device const DecoderGpuSegment *segments [[buffer(1)]],
                  device atomic_uint *pixels [[buffer(2)]],
                  constant DecoderGpuParams &params [[buffer(3)]],
                  device atomic_uint *flags [[buffer(4)]],
                  device const uint2 *info [[buffer(5)]],
                  uint index [[thread_position_in_grid]])
{
    DecoderGpuSegment segment;
    ulong start;
    device atomic_uint *band;
    uint width;
    uint color;
    uint order;
    uint x;
    uint repeat;
    uint p;
    uint q;
    uint ch;
    uint bits;
    uint n;
    uint end;
    uint tag;
    uint value;
    uint top;
    uint row;
    uint i;
    uint k;
    uint found;

    if (index >= params.segments)
        return;
    segment = segments[index];
    width = params.width;
    /* Earlier chunks of the segment give the start column and the nearest
     * selection; without one there, it is the nearest '#' before the
     * segment. */
    color = params.initial_color;
    start = 0ul;
    found = 0u;
    for (k = index; k > 0u && segments[k - 1u].seg == segment.seg; ) {
        --k;
        start += info[k].x;
        if (found == 0u && (info[k].y & 0x100u) != 0u) {
            color = info[k].y & 255u;
            found = 1u;
        }
    }
    if (start > 0x7ffffffful) {
        atomic_fetch_or_explicit(flags, DECODER_GPU_NEEDS_CPU,
                                 memory_order_relaxed);
        return;
    }
    for (q = found != 0u ? 0u : segments[k].start; q > 0u; ) {
        --q;
        if (body[q] == '#') {
            p = q + 1u;
            (void)decoder_gpu_number(body, p, segment.start, value);
            color = value;
            break;
        }
    }
    /* Later chunks of a band paint over earlier ones; the largest tag of a
     * pixel is the last write the serial parser would keep. */
    order = (index + 1u) << 8;
    band = pixels + segment.band * 6u * width;
    x = (uint)start;
    repeat = 1u;
    p = segment.start;
    while (p < segment.end) {
        ch = body[p];
        bits = ch - 63u;
        if (bits <= 63u) {
            ++p;
            n = repeat;
            repeat = 1u;
            if (bits != 0u) {
                top = 31u - clz(bits);
                if (x + n > width || segment.band >= params.bands ||
                        segment.band * 6u + top >= params.height) {
                    atomic_fetch_or_explicit(flags, DECODER_GPU_OUTSIDE,
                                             memory_order_relaxed);
                }
                if (segment.band < params.bands && x < width) {
                    end = min(x + n, width);
                    tag = order | color;
                    for (i = x; i < end; ++i) {
                        for (row = 0u; row < 6u; ++row) {
                            if ((bits >> row) & 1u) {
                                atomic_fetch_max_explicit(
                                    band + row * width + i, tag,
                                    memory_order_relaxed);
                            }
                        }
                    }
                }
            }
            if (x > 0x7fffffffu - n) {
                atomic_fetch_or_explicit(flags, DECODER_GPU_NEEDS_CPU,
                                         memory_order_relaxed);
                return;
            }
            x += n;
        } else if (ch == '#' || ch == '!') {
            ++p;
            if (decoder_gpu_number(body, p, segment.end, value) != 0u) {
                atomic_fetch_or_explicit(flags, DECODER_GPU_NEEDS_CPU,
                                         memory_order_relaxed);
                return;
            }
            if (ch == '#') {
                /* Palette definitions and wide indexes stay on the CPU. */
                if ((p < segment.end && body[p] == ';') || value >= 256u) {
                    atomic_fetch_or_explicit(flags, DECODER_GPU_NEEDS_CPU,
                                             memory_order_relaxed);
                    return;
                }
                color = value;
            } else {
                repeat = value > 0u ? value : 1u;
            }
        } else if (ch == '"' || ch == 0x18u || ch == 0x1au) {
            atomic_fetch_or_explicit(flags, DECODER_GPU_NEEDS_CPU,
                                     memory_order_relaxed);
            return;
        } else {
            ++p;
        }
    }
    /* A repeat carried across a delimiter belongs to the next segment;
     * chunk cuts follow a sixel byte, which consumes it. */
    if (repeat != 1u) {
        atomic_fetch_or_explicit(flags, DECODER_GPU_NEEDS_CPU,
                                 memory_order_relaxed);
    }
}

/* Pack the six resolved pixels of each column into its band-column word. */
/* Accel: DECODE-BAND-PARSE metal */
kernel void
decoder_gpu_pack(device const uint *pixels [[buffer(0)]],
                 device ulong *columns [[buffer(1)]],
                 constant DecoderGpuParams &params [[buffer(2)]],
                 device atomic_uint *flags [[buffer(3)]],
                 uint index [[thread_position_in_grid]])
{
    device const uint *column;
    ulong word;
    ulong need;
    uint width;
    uint band;
    uint rows;
    uint value;
    uint r;

    width = params.width;
    if (index >= width * params.bands)
        return;
    band = index / width;
    column = pixels + band * 6u * width + (index - band * width);
    word = 0ul;
    for (r = 0u; r < 6u; ++r) {
        value = column[r * width];
        if (value != 0u) {
            word |= ((ulong)(value & 255u) << (8u * r)) | (1ul << (48u + r));
        }
    }
    columns[index] = word;
    rows = min(params.height - band * 6u, 6u);
    need = ((1ul << rows) - 1ul) << 48;
    if ((word & need) != need) {
        atomic_fetch_or_explicit(flags, DECODER_GPU_TRANSPARENT,
                                 memory_order_relaxed);
    }
}
