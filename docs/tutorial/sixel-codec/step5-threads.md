# Step 5: Threads

After [step 4](step4-simd.md), both directions are dominated by work that is inherently one byte after another: writing the stream and parsing it. The way forward is to split that work between cores. SIXEL's six-row bands are the natural unit, but the two directions split very differently. The source is [`code/step5.c`](code/step5.c). It includes `step4.c` and reuses its band encoder, its decode loop and its SIMD helpers; its output is byte-identical to steps 2 to 4 for any number of threads.

Set `TUTORIAL_THREADS` to choose the number of threads; by default each program uses up to one per CPU, but no more than the input size justifies, because starting a thread costs more than encoding a small band.

## Encoder: bands are already independent

A band's bytes depend only on its pixels and the palette. The one piece of state that could carry from one band to the next, the selected color, does not: since [step 2](step2-nodes.md#4-emit-pack-lines-greedily), every band starts by selecting its first color. So the encoder cuts the image into contiguous ranges of bands, gives each thread its own range, its own step 4 work area and its own output buffer, and concatenates the buffers in order:

```c
static void *
encode_worker(void *arg)
{
    encode_job_t *job = arg;
    encoder_t e;
    int b;

    job->status = encoder_init(&e, job->img);
    if (job->status != 0) {
        return NULL;
    }
    for (b = job->first_band; b < job->end_band; b++) {
        encode_band(&e, job->img, b * 6, &job->out);
    }
    encoder_free(&e);
    return NULL;
}
```

The calling thread takes the first range, so `n` ranges need only `n - 1` new threads. If a thread cannot be created, its range runs on the calling thread instead. The header, the palette and the concatenation remain serial, but they are small next to the bands.

Contiguous ranges give each thread one large job. They also wait for the slowest thread, and a range of busy bands can take longer than a range of empty ones. libsixel's encoder instead submits [one job per band](../../threading/encoder.md#six-row-jobs-and-ordered-output) to a pool and lets an ordered writer emit finished bands while later ones are still being encoded, which also lets output start before the whole image is done.

## Decoder: find the bands first

A stream is harder to split. Where a band starts is only known by finding the `-` that ends the previous one, a band's first pixels are painted in the color selected somewhere before it, and the image size is only certain at the end. The decoder therefore works in three phases.

### 1. Prescan

One serial pass looks only for the bytes that affect later bands: `-` (a band starts after it), `#` (color selection and definitions) and `"` (raster attributes). Everything else, the sixel data, repeats and carriage returns, is skipped sixteen bytes at a time with the vector compares of step 4:

```c
while (end - p >= 16) {
    v128 v = v_load(p);
    unsigned int hits = v_movemask(v_or(v_or(v_eq(v, v_set1('-')),
                                             v_eq(v, v_set1('#'))),
                                        v_eq(v, v_set1('"'))));
    if (hits != 0) {
        p += ctz64(hits);
        break;
    }
    p += 16;
}
```

For each band, the prescan records its byte offset and the register selected at that point. It also applies every color definition, in stream order, so the palette is final before any thread starts.

### 2. Paint in parallel

The decoder trusts the raster attributes for the image size, allocates column words for every band, and cuts the stream into byte ranges of similar size, each starting at a recorded band start. Each thread runs the step 4 loop over its range, starting with the recorded color, and then transposes its own bands into the image. Ranges cover disjoint bands, so no two threads ever write the same memory and no locks are needed.

### 3. Verify, or decode serially

Raster attributes are a declaration, not a guarantee: they can be missing, and paint can go beyond them. The [decoding pipeline](../../decoder/pipeline/README.md#raster-declaration-versus-observed-paint) defines the image as covering both. A thread that finds paint outside the allocated bands stops and reports it, and a stream without raster attributes never starts the parallel phase. In both cases the serial step 4 decoder decodes the stream again, so the result never depends on the path taken. The test script includes such streams.

libsixel's parallel decoder works on [byte spans](../../threading/decoder.md#why-decoding-is-split-by-bytes) rather than on a list of band starts, but it draws the same kind of boundary: syntax that needs global parser context, such as a palette redefinition or a raster reset, can [send a stream back to serial handling](../../threading/decoder.md#fallback-and-correctness).

## Results

<!-- measurements:begin -->
| Input | Encode | Speed-up | Decode | Speed-up | Stream |
| --- | --- | --- | --- | --- | --- |
| Photograph, 600×450 | 1.05 ms | 1.0× | 0.32 ms | 1.9× | 213,711 bytes |
| Photograph, 2400×1800 | 2.18 ms | 5.8× | 1.98 ms | 3.6× | 2,787,156 bytes |
| Two colors, 2400×1800 | 1.04 ms | 5.5× | 0.63 ms | 5.5× | 1,441,842 bytes |
| Random noise, 2400×1800 | 25.0 ms | 6.5× | 49.8 ms | 1.1× | 19,399,499 bytes |
<!-- measurements:end -->

On the large inputs the encoder runs 5.5 to 6.5 times faster with 14 threads. Several things keep it below 14: the machine has 10 performance and 4 slower efficiency cores, the encoder waits for the slowest of its contiguous ranges, and the header and concatenation remain serial. The small photograph is below the threshold and encodes on one thread.

The decoder runs 3.6 and 5.5 times faster on the large photograph and the two-color pattern, where the prescan skips most bytes sixteen at a time, and twice as fast on the small photograph with three threads. Random noise selects a new color every few bytes, so the prescan parses nearly as many parameters as a full decode, and by Amdahl's law the serial prescan caps the speed-up. A prescan that only searched for `-` and recovered each band's color by searching backwards from it would remove most of that cost, provided the threads applied color definitions in stream order afterwards; this is left as an exercise.

## Where to go from here

The five steps cover the core of a fast SIXEL codec. libsixel adds what a production codec needs around it: transparency and key colors, [OR mode](../../encoder/output/or-mode/README.md), [high-color output](../../encoder/output/high-color/README.md), the size-oriented [encoding policy](../../encoder/output/encode-policy/README.md), palette history and [dequantization](../../decoder/dequantization/README.md) in the decoder, and the overall [threading model](../../threading/README.md) that schedules palette application, encoding and output together.
