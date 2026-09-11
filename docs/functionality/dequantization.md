# Dequantization

Dequantization is optional reconstruction after SIXEL decoding. It mixes selected neighboring colors to reduce visible quantization and dither patterns. It can produce colors outside the decoded palette, but cannot recover the unknown original image or reverse error diffusion losslessly. The default is `none`.

Ordinary palette expansion replaces each index with its palette color and preserves samples. Dequantization changes samples. It is also separate from resizing: the file decoder reconstructs at decoded resolution before applying `sixel2png -s`. See the [decoding pipeline](decoding-pipeline.md), [pixel formats](../concepts/pixelformat.md), and [PNG writer](../writers/png.md) for the surrounding representation boundaries.

## Choosing a method

| Method | Neighborhood and decision | Controls | Intended tradeoff |
| --- | --- | --- | --- |
| `none` | No reconstruction | None | Preserve decoded samples and permit indexed PNG output. |
| `k_undither` or `lso_undither:Vfs` | Eight neighbors with palette-aware similarity weights | `-S`, `-e` | Full neighborhood smoothing with optional edge protection. These names select the same method. |
| `lso_undither:Vlight` | Four neighbors: upper-left, above, upper-right, and left | `-S`; no edge protection | Smaller causal neighborhood; CPU parallel/fused paths and optional GPU execution. It need not match Vfs pixels. |
| `selective_blur` | Fixed 3×3 binomial kernel, gated by center-to-neighbor RGB distance | `threshold`, default 24 | Bounded local work without palette-wide similarity search. |

Vfs refers to the full Floyd–Steinberg-oriented undither neighborhood; it does not run the encoder's diffusion algorithm backward. Vlight is named `fast4` internally. Its neighbors come from decoded source samples, not recursively from already filtered output, allowing independent output rows once the source is available.

For photographs and gradients, compare reconstruction with plain decoding at the intended display size. For pixel art, text, exact palette inspection, and hard-edged diagrams, mixing can remove meaningful detail. No method is guaranteed to improve perceptual quality for every input.

## CLI and configuration

```sh
sixel2png -d none -i input.six -o plain.png
sixel2png -d k_undither -S 100 -e 0 -i input.six -o full.png
sixel2png -d lso_undither:Vlight -i input.six -o light.png
sixel2png -d selective_blur:threshold=24 -i input.six -o blur.png
sixel2png -D -d selective_blur:T24 -i input.six -o rgba.png
```

Compact forms include `k`, `l:Vf`, `l:Vl`, and `s:T24`. Long suboption names use `key=value`, for example `lso_undither:variant=light`. Bare `lso_undither` needs a variant from its suboption or environment; it has no implicit Vfs/Vlight default. `none` explicitly disables reconstruction. `img2sixel -d` selects encoder diffusion instead; it is not this decoder option.

| Setting | Accepted range or values | Default and scope |
| --- | --- | --- |
| `-S`, `--similarity` | Integer 0–1000 | 100; palette-aware methods. Internally zero is clamped to one for similarity calculations. |
| `-e`, `--edge` | Integer 0–1000 | 0; edge protection in the full method. Vlight and selective blur do not use this setting. |
| `selective_blur:threshold=N`, `:TN` | Integer 0–441 | 24; RGB distance gate. |
| `lso_undither:variant=fs`, `:Vfs` | `fs` | Full method, same method ID as `k_undither`. |
| `lso_undither:variant=light`, `:Vlight` | `light` | Four-neighbor method. |

`SIXEL_DEQUANTIZE_LSO_VARIANT` and `SIXEL_DEQUANTIZE_SELECTIVE_BLUR_THRESHOLD` provide the corresponding suboption defaults. The parser initializes defaults, applies environment values for the selected base method, then applies explicit suboptions, so explicit values win. Each successful `-d` parse resolves a fresh method/threshold configuration; an earlier explicit threshold is not a default for a later bare `selective_blur`. Similarity and edge settings are separate decoder state. Full parsing rules and environment validation belong to the [suboption reference](../cli/suboptions.md).

`lsqa -d` reuses the decoder's method/suboption parser, including Vlight and selective blur, for target reconstruction. It does not turn every `lsqa` option into a converter suboption. When measuring quality, record whether reconstruction was applied to the target: a score after smoothing answers a different question from a score for plain decoded SIXEL.

## Algorithm details

### Palette-aware undither

The implementation in [`src/decoder.c`](../../src/decoder.c) caches similarity decisions for palette-color pairs. It compares a pair's midpoint against other palette entries and uses the similarity bias to scale the decision. Thus two identical local color pairs can receive different weights when the rest of the palette changes. `-S` is a heuristic bias, not a blur radius or a percentage of recovered color.

The full method combines a center weight with eight direction-dependent neighbor weights. With edge protection enabled, a Prewitt gradient computed from decoded luminance can increase the center's contribution or preserve a strong-edge sample without smoothing. Vlight retains palette similarity but samples only the four causal neighbors and omits this gradient stage. Both operate on decoded byte RGB values; encoder working-color-space and precision options do not select a different reconstruction color space.

### Selective blur

For each painted center, the filter starts with center weight 4. It considers eight neighbors with these spatial weights:

```text
1  2  1
2  4  2
1  2  1
```

A neighbor is included only when `dr*dr + dg*dg + db*db <= threshold*threshold`, using decoded 8-bit RGB differences. Out-of-image and unpainted neighbors are omitted, and the accepted weights are renormalized. Each output channel is the integer weighted sum divided by the accepted weight sum. There is no resize, iterative feedback, perceptual-distance transform, or palette-wide search in this filter.

Threshold zero allows only identical colors, preserving valid painted RGB samples. A larger threshold admits more color differences but does not change the 3×3 radius. For a one-row image with red values 100 and 112 followed by blue `(0,0,255)`, threshold 24 produces red values 104 and 108 and preserves the blue sample; threshold zero preserves all three samples. This exact example is covered by DQ-05. The upper bound 441 is an integer policy limit, not a promise to admit the maximum black-to-white RGB distance, which is slightly larger than 441.

## Transparency and palette history

The stateful packed-pixel path reconstructs from indexes, palette, and a paint mask into RGBA. Unpainted centers remain transparent, and unpainted neighbors do not contribute color to painted centers. Packing into alpha-bearing formats retains alpha; RGB and X-channel formats composite over the caller's background as described by the decoding pipeline. Do not infer opacity merely from enabling a filter.

The file path normally produces RGB888 after reconstruction. `sixel2png -D` requests RGBA output and retains the direct decoder's alpha plane. This distinction matters for sparse SIXEL: the historical RGB PNG path cannot express its unpainted pixels as transparency. Reconstruction can also change PNG output from indexed color type 3 to RGB type 2 or RGBA type 6 and can change compressed size.

A stream that redefines palette registers after painting cannot be reconstructed correctly from one index plane and its final palette. The stateful decoder detects `SIXEL_DECODE_PIXELS_RESULT_PALETTE_REDEFINED`, bypasses reconstruction, and retains paint-time colors through direct decoding. An acceleration attempt may occur before this decision, but its reconstructed result is not used when the redefinition flag requires the bypass. This is a correctness boundary, not a quality optimization.

## Library integration

Use the public stateful API declared in [`include/sixel.h.in`](../../include/sixel.h.in): create a decoder with `sixel_decoder_new()`, configure it with `sixel_decoder_setopt(decoder, SIXEL_OPTFLAG_DEQUANTIZE, "lso_undither:Vlight")`, then call `sixel_decoder_decode_pixels()` for in-memory packed output or `sixel_decoder_decode()` for file conversion. Check every status before continuing. Similarity and edge use `SIXEL_OPTFLAG_SIMILARITY` and `SIXEL_OPTFLAG_EDGE` with string values.

Initialize `sixel_decode_options_t` and `sixel_decode_result_t`, set the preferred pixel format when needed, and consume the returned dimensions, stride, format, and flags. Free returned pixels using the matching allocator, then release the decoder and allocator references. The stateless `sixel_decode_pixels()` and raw/direct parsing APIs do not acquire dequantization from a decoder object. Internal `sixel_dequantize_*` helpers in [`src/decoder.h`](../../src/decoder.h) are implementation seams, not the installed application interface.

## CPU, GPU, and cost

Full undither without edge protection and Vlight have parallel CPU paths; setup or eligibility can select scalar execution. Vlight also has a fused CPU decode path. The full edge-protected method needs gradient work, and selective blur uses its own local filter. A thread count is not a promise that every reconstruction stage runs concurrently. See [decoder threading](../threading/decoder.md).

GPU policy defaults to `off` unless configured through the environment or decoder options. The optional GPU dequantizer currently supports Vlight only, through Metal. `--gpu=auto` permits CPU fallback, while `--gpu=force` requires supported execution and reports errors when the requested method or engine cannot run. For example, selective blur with `--gpu=force` is rejected even though its kernel is conceptually suitable for GPU implementation.

`--gpu=auto:dequant_threshold=262144` expresses the default automatic pixel-count cutoff; `SIXEL_GPU_DEQUANT_THRESHOLD` supplies its environment counterpart. The threshold controls when an attempt is eligible, not guaranteed speedup. GPU dispatch consumes already decoded RGBA plus a palette, writes a second RGBA buffer, and still examines palette colors for similarity. It does not parse SIXEL on the GPU or fuse reconstruction with resizing. The owning interfaces are [`src/gpu-dequant.h`](../../src/gpu-dequant.h) and [`src/gpu-dequant.c`](../../src/gpu-dequant.c).

For `P` pixels and `K` palette entries, selective blur does a bounded number of neighbor operations per pixel. CPU palette-aware filtering additionally uses a `K × K` similarity cache; an uncached pair can scan the palette. Output buffers, gradient buffers on the scalar full path, parallel setup, and GPU source/destination storage contribute to peak memory. Final PNG size is not a memory estimate. Measure decode, reconstruction, resizing, and PNG writing separately before attributing end-to-end cost to the filter.

## Test coverage

<!-- test-coverage: enforced -->

### Behavioral contract tests

| ID | Observation | Owning test |
| --- | --- | --- |
| DQ-01 | Full undither default RGB samples are fixed at similarity 100 and edge 0. | [tests/processing/decoder/0010_decoder_kundither_default.t](../../tests/processing/decoder/0010_decoder_kundither_default.t) |
| DQ-02 | Changing similarity bias has the expected sample-level effect. | [tests/processing/decoder/0011_decoder_kundither_similarity.t](../../tests/processing/decoder/0011_decoder_kundither_similarity.t) |
| DQ-03 | Full-method edge protection has the expected sample-level effect. | [tests/processing/decoder/0012_decoder_kundither_edge.t](../../tests/processing/decoder/0012_decoder_kundither_edge.t) |
| DQ-04 | Packed output preserves alpha and excludes transparent neighbors across supported reconstruction methods. | [tests/processing/decoder/0008_decoder_pixels_output_formats.t](../../tests/processing/decoder/0008_decoder_pixels_output_formats.t) |
| DQ-05 | Selective blur thresholds 0 and 24 produce the documented exact RGB values. | [tests/processing/decoder/0021_decoder_selective_blur_threshold.t](../../tests/processing/decoder/0021_decoder_selective_blur_threshold.t) |
| DQ-06 | Palette redefinition reports its flag and preserves paint-time colors despite reconstruction requests. | [tests/processing/decoder/0023_decoder_high_color_dequantize_bypass.t](../../tests/processing/decoder/0023_decoder_high_color_dequantize_bypass.t) |
| DQ-07 | Parallel full undither matches scalar output. | [tests/processing/decoder/0013_decoder_kundither_parallel_matches_scalar.t](../../tests/processing/decoder/0013_decoder_kundither_parallel_matches_scalar.t) |
| DQ-08 | Parallel Vlight matches scalar output. | [tests/processing/decoder/0014_decoder_kundither_fast4_parallel_matches_scalar.t](../../tests/processing/decoder/0014_decoder_kundither_fast4_parallel_matches_scalar.t) |
| DQ-09 | Fused Vlight matches scalar output. | [tests/processing/decoder/0015_decoder_kundither_fast4_fused_matches_scalar.t](../../tests/processing/decoder/0015_decoder_kundither_fast4_fused_matches_scalar.t) |
| DQ-10 | The selective-blur environment threshold matches its explicit CLI equivalent. | [tests/cli/core/0030_basic_dequantize_selective_blur_threshold_env.t](../../tests/cli/core/0030_basic_dequantize_selective_blur_threshold_env.t) |
| DQ-11 | The LSO variant environment setting matches its explicit CLI equivalent. | [tests/cli/core/0029_basic_dequantize_lso_variant_env.t](../../tests/cli/core/0029_basic_dequantize_lso_variant_env.t) |
| DQ-12 | Selective blur rejects forced GPU execution. | [tests/cli/core/0028_basic_dequantize_selective_blur_gpu_force_reject.t](../../tests/cli/core/0028_basic_dequantize_selective_blur_gpu_force_reject.t) |
| DQ-13 | Representative dequantized PNG output has RGB8 IHDR. | [tests/writer/png/0002_rgb8_ihdr.t](../../tests/writer/png/0002_rgb8_ihdr.t) |
| DQ-14 | The lsqa parser accepts compact selective-blur configuration. | [tests/cli/core/0027_basic_lsqa_dequantize_selective_blur_compact.t](../../tests/cli/core/0027_basic_lsqa_dequantize_selective_blur_compact.t) |

### Quality regression tests

The [quality-gate suite](../../tests/quality_gate/) includes reconstruction-aware target evaluation. These thresholds protect their stated fixtures and metrics; they do not prove recovery of the original image or universal improvement. Exact scalar/parallel equivalence above is a separate observation from perceptual quality.

### Defensive and malformed-input tests

The [decoder tests](../../tests/processing/decoder/) and [GPU dequantization tests](../../tests/processing/gpu-dequant/) complement parser/security coverage with internal boundaries and dispatch configuration checks. Their presence does not establish exhaustive malformed-input, allocation-failure, or backend coverage.

### Coverage boundary

The linked tests establish the listed observations, not every combination described by the implementation. Remaining focused coverage opportunities include repeated `-d` state, explicit-versus-environment precedence for each method, exhaustive threshold endpoints, GPU/CPU numerical comparisons on available hardware, and file-output transparency across every method. Build-conditioned skips are not successful hardware validation. No new performance benchmark or visual-quality ranking is claimed here.
