# Lookup policy measurements

This page holds the evidence behind the measured claims of the [lookup policy](../README.md) reference: color error, MS-SSIM, and end-to-end speed of each policy across palette sizes, with and without error diffusion. It is written for contributors who evaluate or change a lookup policy. The lookup policy reference states the contracts; this page only measures them.

## Questions

The historical RGB555 path reduces each byte-valued color axis from 256 values to 32 histogram and cache addresses. This section asks seven user-visible questions:

1. How do current policies compare when palette construction is controlled by `-Qkmeans:Gw -Xoklab`?
2. How does the current Heckbert compatibility mapping expose the legacy RGB555 limitation alongside policies that were added later?
3. What end-to-end speed does each policy deliver under the same modern CLI configuration?
4. Does sharing the `5bit`, `6bit`, or `certlut` instance between workers materially change end-to-end latency?
5. What latency change does the Metal PaletteApply path produce for its supported `none` and `eytzinger` policies?
6. How do the nine policies change when Floyd--Steinberg error diffusion is enabled instead of disabling dithering?
7. How does `delaunay` compare with `vptree`, `5bit`, and `6bit` across byte and native-float lookup, three images, and both diffusion settings?

The nine-policy measurements answer the first six questions for one fixture. The separate Delaunay comparison answers the seventh for three fixtures. Neither establishes a universal ranking of lookup policies.

Unless a subsection explicitly says otherwise, every quality and speed result below uses `--diffusion=none`. This isolates palette construction and lookup more clearly by removing spatial error feedback. It is not representative of the default experience when a user enables dithering; the dedicated Floyd--Steinberg control below measures that case separately.

The nine-policy curves labeled 8-bit resolve to `work=rgb888`. This is part of the protocol because the same lookup-policy name selects different backends and cost structures on `rgb-f32`. The Delaunay comparison records the selected `lookup/POLICY.8bit` or `lookup/POLICY.float32` class for each case; `--precision=8bit` alone does not establish lookup representation when CMS promotes an input. The matched `K=64` comparison is in [Encoder Working Precision](../../../precision/README.md#lookup-policies); it complements, rather than replaces, the palette-size sweeps below.

## Conditions

The nine-policy no-diffusion and Floyd--Steinberg curves, shared-instance curves, and Metal curves were measured on 2026-09-06 from clean Autotools builds of revision `f2c138d8f` on macOS 26.5.1 arm64. The input was [`images/snake.png`](../../../../../images/snake.png). The broad curves use `K = 8, 16, 32, 64, 128, 256`; both focused curves use steps of 16 from 128 through 256. The Delaunay study has its own source snapshot, conditions, and figures below; its times must not be spliced into these older curves.

The controlled K-means quality and speed comparisons use this normal CLI path:

```text
img2sixel \
  --threads=1 --precision=8bit --quality=full \
  --loaders=libpng! \
  --quantize-model=kmeans:seed=1 --merge-policy=ward -Xoklab -Wgamma \
  --diffusion=none --gpu-policy=off \
  --lookup-policy=POLICY -p K \
  images/snake.png
```

The Floyd--Steinberg control changes only the diffusion option:

```text
img2sixel \
  --threads=1 --precision=8bit --quality=full \
  --loaders=libpng! \
  --quantize-model=kmeans:seed=1 --merge-policy=ward -Xoklab -Wgamma \
  --diffusion=fs --gpu-policy=off \
  --lookup-policy=POLICY -p K \
  images/snake.png
```

The current Heckbert compatibility comparison instead uses:

```text
img2sixel \
  --threads=1 --precision=8bit --quality=full \
  --loaders=libpng! \
  --quantize-model=heckbert --cover-policy=off --merge-policy=none \
  --diffusion=none --gpu-policy=off \
  --lookup-policy=POLICY -p K \
  images/snake.png
```

The shared-instance speed comparison uses controlled K-means, eight total pipeline threads, and explicit private or shared instances:

```text
img2sixel \
  --threads=8 --precision=8bit --quality=full \
  --loaders=libpng! \
  --quantize-model=kmeans:seed=1 --merge-policy=ward -Xoklab -Wgamma \
  --diffusion=none:band_overwrap=0 --gpu-policy=off \
  --lookup-policy=POLICY:shared_instance=0|1 -p K \
  images/snake.png
```

The Metal comparison uses one CPU thread and forces the GPU PaletteApply path:

```text
img2sixel \
  --threads=1 --precision=8bit --quality=full \
  --loaders=libpng! \
  --quantize-model=heckbert --cover-policy=off --merge-policy=none \
  --diffusion=none:band_overwrap=0 --gpu-policy=off|force \
  --lookup-policy=none|eytzinger -p K \
  images/snake.png
```

`lsqa` compares each SIXEL stream with the original image and reports its existing `Δ E00_mean`, `Δ Chroma_mean`, and MS-SSIM metrics. Encoder input is pinned to the `libpng` loader, and the runner removes inherited `SIXEL_*` variables from both programs. The K-means seed is explicitly one. The baseline disables diffusion so propagated error does not obscure the palette and lookup effects; the control changes only that option to `fs`. One worker exercises the serial lazy-cache behavior of `5bit` and `6bit`.

## Results

### Controlled K-means result

![Mean Delta E00 and mean absolute CIELAB chroma error for the controlled K-means lookup-policy comparison](lookup-policy-kmeans-color-error.png)

Lower values are better on both panels. The complete measurements are in [`lookup-policy-kmeans-color-error.csv`](lookup-policy-kmeans-color-error.csv). The corresponding spatially pooled view is [MS-SSIM](lookup-policy-kmeans-ms-ssim.png), with values in [`lookup-policy-kmeans-ms-ssim.csv`](lookup-policy-kmeans-ms-ssim.csv).

At `K = 256`, the measured values are:

| Policy | Mean Delta E00 | Mean absolute chroma error | MS-SSIM |
| --- | ---: | ---: | ---: |
| `none` | 1.904815 | 1.540278 | 0.989294 |
| `5bit` | 2.305358 | 1.876720 | 0.981868 |
| `6bit` | 2.028380 | 1.625482 | 0.987094 |
| `certlut` | 1.904373 | 1.544062 | 0.989328 |
| `eytzinger` | 2.186592 | 1.943040 | 0.989361 |
| `fhedt` | 2.003165 | 1.646098 | 0.988420 |
| `vptree` | 1.905966 | 1.541044 | 0.989289 |
| `rbc` | 1.904815 | 1.540278 | 0.989294 |
| `mahalanobis` | 1.904815 | 1.540278 | 0.989294 |

`-Qkmeans:Gw` selects K-means with Ward final merging, while `-Xoklab` places its clustering objective in OKLab. `-Wgamma` keeps palette application in byte-valued gamma RGB so this figure does not mix in a working-colorspace comparison. K-means still finds a local solution rather than a guaranteed global optimum.

The measured `6bit` result is coarser than `none`, as its bucket memoization permits, but it is materially closer than `5bit`. On this fixture at `K = 256`, its mean Delta E00 is 6.5 percent above `none`, while `5bit` is 21.0 percent above `none`. This supports RGB666 as a substantially better approximation than RGB555, but still does not make it output-equivalent to direct lookup.

`certlut`, `vptree`, `rbc`, and `mahalanobis` remain at or very near the direct result on this fixture. Their mean Delta E00 values span 1.904373 through 1.905966, with end-to-end medians from 85.4 through 114.6 ms. `fhedt` is faster at 82.3 ms but reaches 2.003165 mean Delta E00; Eytzinger is similarly fast at 83.1 ms but reaches 2.186592. The result exposes several distinct quality/speed tradeoffs rather than one uniformly best accelerated policy.

This section has no fixed-palette comparison. Such a comparison must apply each explicit lookup policy to the supplied `-m` palette, which the CLI does, and has not been measured.

### Legacy RGB555 context in current Heckbert mode

The current Heckbert comparison is retained because it exposes the palette construction coupling associated with preserving the legacy RGB555 path. It is not a reconstruction of a historical user choice: through the 1.8.7 era, the RGB555 route was fixed and users could not select a lookup policy.

![Mean Delta E00 and mean absolute CIELAB chroma error when each lookup policy also selects Heckbert histogram resolution](lookup-policy-color-error.png)

At `K = 8`, `none` reported mean absolute chroma error 8.435487, while `5bit`, `6bit`, `certlut`, `eytzinger`, and `vptree` reported 14.914233, 16.088598, 16.104532, 16.587467, and 16.105232 respectively. The later `fhedt`, `rbc`, and `mahalanobis` policies reported 15.895770, 16.106705, and 16.106705.

The current compatibility mapping is in [`histogram_control_make_for_policy`](../../../../../src/palette-heckbert.c#L403). The Heckbert builder uses eight bits per channel for `none`, five for `5bit`, and six for the other policies in this comparison. The non-`5bit` branches are modern comparison points, not modes that the old CLI exposed. Their histograms change the box boundaries, weighted medians, and representative colors before palette application begins. Inspection of the decoded `K = 8` outputs found that the most frequently selected `none` entry was muted brown RGB `(125, 92, 87)`, with CIELAB chroma about 15.1. The six-bit palette instead most frequently selected olive RGB `(130, 128, 38)`, with chroma about 47.9. Large low- and medium-chroma regions of this particular image were therefore oversaturated by the six-bit palette.

The explanation is fixture-specific: `none` happened to receive a palette whose chroma distribution better matched this image. It is not evidence that an exhaustive RGB lookup intrinsically minimizes CIELAB chroma error.

The companion spatially pooled measurement for this current comparison is:

![MS-SSIM for the current Heckbert compatibility comparison](lookup-policy-ms-ssim.png)

At `K = 256`, `5bit` reaches 0.973219 while `none` reaches 0.986088. The underlying values are in [`lookup-policy-ms-ssim.csv`](lookup-policy-ms-ssim.csv).

### Controlled K-means high-palette-size detail

![Focused mean Delta E00 and mean absolute CIELAB chroma error for the controlled K-means comparison from K 128 through 256](lookup-policy-kmeans-high-k.png)

The focused source data are in [`lookup-policy-kmeans-high-k.csv`](lookup-policy-kmeans-high-k.csv). At `K = 256`, the measured values are:

| Policy | Mean Delta E00 | Mean absolute chroma error |
| --- | ---: | ---: |
| `none` | 1.904815 | 1.540278 |
| `5bit` | 2.305358 | 1.876720 |
| `6bit` | 2.028380 | 1.625482 |
| `certlut` | 1.904373 | 1.544062 |
| `eytzinger` | 2.186592 | 1.943040 |
| `fhedt` | 2.003165 | 1.646098 |
| `vptree` | 1.905966 | 1.541044 |
| `rbc` | 1.904815 | 1.540278 |
| `mahalanobis` | 1.904815 | 1.540278 |

This focused figure uses the same `-Qkmeans:Gw -Xoklab -Wgamma` condition as the broad controlled comparison. It therefore enlarges the 128--256 region without introducing the policy-dependent Heckbert histogram resolution. `certlut`, `vptree`, `rbc`, and `mahalanobis` stay near the direct result in this range, while the separate speed figure shows the lower latency and larger error of `fhedt` and `eytzinger`.

### Palette-geometry sensitivity under Heckbert

![Focused mean Delta E00 and mean absolute CIELAB chroma error for the Heckbert palette-geometry sensitivity comparison from K 128 through 256](lookup-policy-heckbert-high-k.png)

The focused source data are in [`lookup-policy-heckbert-high-k.csv`](lookup-policy-heckbert-high-k.csv). At `K = 256`, the measured values are:

| Policy | Mean Delta E00 | Mean absolute chroma error |
| --- | ---: | ---: |
| `none` | 2.447771 | 1.842988 |
| `5bit` | 3.155706 | 2.269776 |
| `6bit` | 2.596776 | 1.859324 |
| `certlut` | 2.403138 | 1.749753 |
| `eytzinger` | 3.032645 | 2.461793 |
| `fhedt` | 2.475566 | 1.747344 |
| `vptree` | 2.400774 | 1.748358 |
| `rbc` | 2.402073 | 1.756719 |
| `mahalanobis` | 2.402073 | 1.756719 |

#### Why some Heckbert configurations beat `none`

Several non-`none` configurations score better than `none` in this end-to-end Heckbert measurement. At `K = 256`, `vptree`, `rbc`, `mahalanobis`, and `certlut` report mean Delta E00 values from 2.400774 to 2.403138, below `none` at 2.447771. For mean chroma error, `fhedt`, `vptree`, `certlut`, `rbc`, and `mahalanobis` are also below `none`.

This result does not mean that a non-`none` lookup is more exact than an exhaustive scan over the same palette. In the current Heckbert compatibility path, `none` constructs its palette from a `256^3` histogram, whereas these listed policy configurations use a `64^3` histogram. Their palettes have already diverged before lookup begins. In addition, `none` minimizes squared distance in stored gamma-RGB coordinates, while the plotted metrics are Delta E00 and CIELAB chroma error. Exactness for the lookup metric therefore does not guarantee the lowest value for either plotted perceptual metric.

The important observation is that finer histogram resolution plus exhaustive RGB lookup does not monotonically improve end-to-end perceptual error. The coarser histogram may act as a useful regularizer for this fixture by changing Heckbert's splits and representative colors, but this graph does not isolate or prove that mechanism. A fixed-palette component benchmark is required to compare lookup error independently of palette construction.

Palette quality and lookup indexability are separate properties. In a `K = 256` diagnostic, `eytzinger` and `vptree` emitted identical palette definitions within each quantizer, yet their output quality depended strongly on the palette geometry. Under controlled K-means their mean Delta E00 values are 2.186592 and 1.905966, a difference of 0.280626. Under Heckbert they are 3.032645 and 2.400774, a difference of 0.631871. The Heckbert palette is not simply worse: with `vptree` it scores better than the K-means palette on this fixture.

The 8-bit `eytzinger` policy projects colors with `r + g + b` and examines a fixed window of at most six palette entries on each side of the insertion position. A palette whose nearest-neighbor geometry is poorly preserved by that projection exposes the approximation. `vptree` instead uses metric branch-and-bound tests and retains its nearest-distance guarantee. This pair of figures therefore demonstrates why lookup policies should be evaluated with more than one palette geometry. It does not establish that either quantizer is generally easier or harder to search from this single image.

### End-to-end speed

![Median end-to-end runtime for each lookup policy](lookup-policy-speed.png)

The figure reports median elapsed time; lower is better. Each point is the median of nine fresh processes after two warm-up runs, and the error bars show the interquartile range. The source data and complete command template are in [`lookup-policy-speed.csv`](lookup-policy-speed.csv).

At `K = 256`, the result is:

| Policy | Median time | Speedup over `none` |
| --- | ---: | ---: |
| `none` | 128.3 ms | 1.00x |
| `5bit` | 70.3 ms | 1.83x |
| `6bit` | 72.0 ms | 1.78x |
| `certlut` | 85.4 ms | 1.50x |
| `eytzinger` | 83.1 ms | 1.54x |
| `fhedt` | 82.3 ms | 1.56x |
| `vptree` | 88.1 ms | 1.46x |
| `rbc` | 114.6 ms | 1.12x |
| `mahalanobis` | 113.4 ms | 1.13x |

This confirms a real speed/accuracy tradeoff between `6bit` and `none` on the fixture. It does not show that `6bit` is the best current default: both the exactness properties and the measured runtime of the other accelerated policies must be considered. Dense `5bit` and `6bit` tables also pay fixed initialization and cold-bucket costs, so their relative position changes with `K`, image size, cache reuse, and traversal order.

The true 8-bit result changes the shape seen in the earlier float-promoted run. From `K = 16` through `K = 256`, `fhedt` rises from 55.6 ms to 82.3 ms, while the dense `5bit` and `6bit` paths rise from about 49.6 ms to 70.3 and 72.0 ms. Fhedt remains relatively shallow and close to Eytzinger, but it no longer outruns the byte-addressed tables at high `K`. The figure is an end-to-end measurement, so a component benchmark is still required to assign the difference solely to lookup.

### Shared-instance performance

![Median end-to-end runtime with private and shared lookup instances](lookup-policy-shared-speed.png)

This focused comparison holds the process thread limit at eight and compares `shared_instance=0` (`S0`, one lookup instance per dither worker) with `shared_instance=1` (`S1`, one instance shared by the workers). It covers only the three policies that expose this switch. The source data are in [`lookup-policy-shared-speed.csv`](lookup-policy-shared-speed.csv).

At `K = 256`, the result is:

| Policy | `S0` median | `S1` median | `S1` change from `S0` |
| --- | ---: | ---: | ---: |
| `5bit` | 59.3 ms | TODO(remeasure) | TODO(remeasure) |
| `6bit` | 59.6 ms | TODO(remeasure) | TODO(remeasure) |
| `certlut` | 59.9 ms | 59.9 ms | -0.1% |

These checked artifacts were recorded at source revision `f2c138d8f6aa0a90c89d019f85eaf9bb3efa5edb`, before worker-local `5bit` and `6bit` instances populated their dense tables during parallel mapping. At that revision, the private and shared curves were mostly within each other's interquartile ranges, so the run did not establish a material end-to-end latency benefit for either setting.

The shared `5bit` and `6bit` values are `TODO(remeasure)`. Shared instances populate their dense tables with relaxed atomic accesses since commit `30820aa9b`; the checked graph and CSV describe the read-only shared cache of the recorded revision and must not be used to characterize the current shared path.

The runner that recorded these artifacts also required `S0` and `S1` to produce byte-identical SIXEL at `K = 256` for all three policies. At that revision the equality was a consequence of a defect in worker-local cache writes, not a contract. The current reproduction runner requires `S0` and `S1` to differ for `5bit` and `6bit`, while `certlut` remains byte-identical. Diffusion is disabled and band overlap is fixed at zero. Regenerate these artifacts before using the recorded table to characterize current dense-cache performance. Shared `5bit` and `6bit` output can also vary between runs because competing first writers can memoize different valid representatives. This is intentionally not a thread-scaling benchmark and does not measure the quality effect of parallel-band error diffusion.

### Metal PaletteApply performance

![Median end-to-end runtime on CPU and forced Metal](lookup-policy-metal-speed.png)

The Metal comparison covers the two implemented GPU lookup modes, `none` and `eytzinger`. CPU points use `--gpu-policy=off`; Metal points use `--gpu-policy=force`, so an unsupported request, missing Metal device, or GPU failure makes the run fail instead of silently falling back. The source data are in [`lookup-policy-metal-speed.csv`](lookup-policy-metal-speed.csv).

At `K = 256`, the result is:

| Policy | CPU median | Metal median | CPU / Metal |
| --- | ---: | ---: | ---: |
| `none` | 103.0 ms | 79.4 ms | 1.30x |
| `eytzinger` | 51.7 ms | 70.9 ms | 0.73x |

On this 600-by-450, 270,000-pixel input, Metal `none` first overtakes CPU `none` near the high end of the measured palette range: at `K = 128` the medians are 71.1 and 74.3 ms, while at `K = 256` Metal is 23.0 percent faster. For `eytzinger`, forced Metal is slower at every measured `K`; at `K = 256` it is 37.2 percent slower. Direct `none` retains an `O(P K)` palette scan whose larger `K` can amortize Metal setup and transfer costs. The CPU Eytzinger query already reduces lookup work enough that this one-shot 270,000-pixel run does not amortize those costs.

This does not establish that CPU Eytzinger is faster for larger images or a long-lived process that reuses Metal state. Each sample starts a fresh `img2sixel` process, so process startup, Metal initialization, palette construction, transfers, and SIXEL encoding all remain in the measurement. It does show that pixel count alone is insufficient to predict a profitable GPU dispatch: palette size and CPU lookup policy matter too. The current `--gpu-policy=auto` threshold is based on pixel count, not `K`.

The controlled K-means path used by the other speed figures currently completes palette application outside this Metal dispatch. The GPU comparison therefore uses `-Q heckbert -a off -F none` so it exercises PaletteApply. Each CPU/Metal pair uses the same policy and generated palette. Comparisons between the `none` and `eytzinger` pairs still include Heckbert's policy-dependent histogram resolution and must not be read as an isolated lookup microbenchmark.

At `K = 256`, CPU and Metal output was byte-identical within each policy. The manifest records the output sizes and SHA-256 digests, as well as the detected 30-core Apple M3 Max and Metal inventory.

### Floyd--Steinberg control

![Mean Delta E00 and mean absolute CIELAB chroma error with Floyd--Steinberg diffusion](lookup-policy-kmeans-fs-color-error.png)

This control repeats the broad controlled K-means comparison with only the diffusion setting changed from `none` to `fs`. The complete color-error values are in [`lookup-policy-kmeans-fs-color-error.csv`](lookup-policy-kmeans-fs-color-error.csv). The corresponding spatially pooled view is [MS-SSIM](lookup-policy-kmeans-fs-ms-ssim.png), with values in [`lookup-policy-kmeans-fs-ms-ssim.csv`](lookup-policy-kmeans-fs-ms-ssim.csv).

At `K = 256`, changing only `--diffusion=none` to `--diffusion=fs` produced:

| Policy | Mean Delta E00 (`none` -> `fs`) | Mean absolute chroma error (`none` -> `fs`) | MS-SSIM (`none` -> `fs`) |
| --- | ---: | ---: | ---: |
| `none` | 1.904815 -> 2.162113 | 1.540278 -> 1.833049 | 0.989294 -> 0.992358 |
| `5bit` | 2.305358 -> 2.448471 | 1.876720 -> 2.062422 | 0.981868 -> 0.989076 |
| `6bit` | 2.028380 -> 2.238910 | 1.625482 -> 1.902066 | 0.987094 -> 0.991549 |
| `certlut` | 1.904373 -> 2.160246 | 1.544062 -> 1.831859 | 0.989328 -> 0.992399 |
| `eytzinger` | 2.186592 -> 2.473524 | 1.943040 -> 2.309031 | 0.989361 -> 0.992292 |
| `fhedt` | 2.003165 -> 2.250221 | 1.646098 -> 1.895945 | 0.988420 -> 0.991588 |
| `vptree` | 1.905966 -> 2.161557 | 1.541044 -> 1.832612 | 0.989289 -> 0.992368 |
| `rbc` | 1.904815 -> 2.162113 | 1.540278 -> 1.833049 | 0.989294 -> 0.992358 |
| `mahalanobis` | 1.904815 -> 2.162113 | 1.540278 -> 1.833049 | 0.989294 -> 0.992358 |

Mean per-pixel Delta E00 and chroma error increased for every policy. That is not a contradiction: Floyd--Steinberg redistributes quantization error spatially rather than minimizing the independent error of each pixel. MS-SSIM increased for every policy in this true 8-bit run, especially for `5bit`. Similar six-decimal aggregate values for several policies do not by themselves establish byte-identical or pixel-identical output.

![Median end-to-end runtime for each lookup policy with Floyd--Steinberg diffusion](lookup-policy-fs-speed.png)

The timing data are in [`lookup-policy-fs-speed.csv`](lookup-policy-fs-speed.csv).

At `K = 256`, the timing comparison is:

| Policy | No-diffusion median | FS median | FS median change |
| --- | ---: | ---: | ---: |
| `none` | 128.3 ms | 131.6 ms | +2.6% |
| `5bit` | 70.3 ms | 75.9 ms | +8.0% |
| `6bit` | 72.0 ms | 80.9 ms | +12.3% |
| `certlut` | 85.4 ms | 108.0 ms | +26.6% |
| `eytzinger` | 83.1 ms | 89.8 ms | +8.1% |
| `fhedt` | 82.3 ms | 87.2 ms | +6.0% |
| `vptree` | 88.1 ms | 103.5 ms | +17.5% |
| `rbc` | 114.6 ms | 119.3 ms | +4.0% |
| `mahalanobis` | 113.4 ms | 119.3 ms | +5.3% |

These are end-to-end results: they include diffusion arithmetic and every policy-dependent change to the error propagated into later pixels. They do not isolate nearest-neighbor query cost, and they must not be compared with the no-diffusion curves as though diffusion were a constant additive overhead. The dither-adjusted color stream can also change lookup-cache locality and the work performed by SIXEL encoding. In this run that effect is most visible for `certlut`, `vptree`, and `6bit`; the table does not by itself assign the timing change to one component.

### Delaunay: byte and native-float comparison

This study compares [`delaunay`](../delaunay.md), `vptree`, `5bit`, and `6bit` at `K = 32, 64, 128, 256`, with both `fs` and `none` diffusion. It uses [`snake.png`](../../../../../images/snake.png) and [`egret.jpg`](../../../../../images/egret.jpg) at 600 by 450 pixels, plus [`vimperator3.png`](../../../../../images/vimperator3.png) at 582 by 746 pixels. Egret is converted once to an 8-bit RGB PNG before timing so every input uses the same `libpng` loader. There are 192 timed configurations and 48 paired Delaunay/VP-tree output checks.

The run was recorded on 2026-10-22 JST on an Apple M3 Max with 36 GiB RAM, macOS 27.0.1 arm64, and Apple Clang 21.0.0. The implementation is identified by clean local measurement snapshot `d1acd91280d9e97247aaf6f2f96abe7b8e60345b`, based on repository revision `50bcca8f391b71bea52a7cf85d5174432720a104`. The snapshot contains the Delaunay implementation under evaluation; its ID identifies a local measurement commit, not a published release or branch. The [manifest](lookup-policy-delaunay-run.json) records its tree ID, compiler and configure arguments, input and executable/library hashes, effective lookup classes, all timing samples, and paired output hashes. Autotools used `CFLAGS=-O3` with the project's C99 flag. Byte lookup used NEON; `FLOAT_LOOKUP_CFLAGS` was empty, so float lookup retained native scalar arithmetic under the existing [SIMD gate](../delaunay.md#method).

Every command uses `--threads=1 --precision=8bit --quality=full --loaders=libpng! --quantize-model=kmeans:seed=1 --merge-policy=ward -Wgamma --gpu-policy=off`. The byte matrix adds `--cms-engine=none`; the float matrix keeps default CMS and adds `-Xoklab`. All inherited `SIXEL_*` variables are removed. An untimed `LSXLUT2|phase=palette-apply` trace confirms `lookup/POLICY.8bit` or `lookup/POLICY.float32` for each configuration. CMS and clustering differ between the two matrices, so comparisons are within each representation, not an estimate of the cost of changing representation alone.

Hyperfine runs each configuration with `-N`, two warmups and seven measured fresh processes. The plots show arithmetic mean elapsed time with sample-standard-deviation error bars; the [CSV](lookup-policy-delaunay-speed.csv) also retains medians and extrema, and the [JSON](lookup-policy-delaunay-run.json) retains every sample. Output goes to the null device during timing. Policy order is fixed as `vptree`, `delaunay`, `5bit`, `6bit`, without round-by-round rotation. Process startup, loading, palette construction, lookup preparation, mapping, and SIXEL encoding are all included.

![Mean end-to-end runtime with standard-deviation bars for byte lookup, three images, and two diffusion settings](lookup-policy-delaunay-8bit-speed.png)

![Mean end-to-end runtime with standard-deviation bars for native-float lookup, three images, and two diffusion settings](lookup-policy-delaunay-float32-speed.png)

The following ratios divide each Delaunay mean by its matched VP-tree mean. Each geometric mean gives equal weight to 12 configurations, comprising three images and four palette sizes; a ratio below one means less elapsed time.

| Lookup representation | Diffusion | Geometric mean ratio | Range of individual ratios |
| --- | --- | ---: | ---: |
| Byte | Floyd--Steinberg | 0.946 | 0.882--1.028 |
| Byte | None | 1.014 | 0.913--1.163 |
| Native float | Floyd--Steinberg | 0.972 | 0.885--1.067 |
| Native float | None | 1.003 | 0.880--1.060 |

Delaunay has lower aggregate elapsed time with Floyd--Steinberg in this run, while the no-diffusion aggregate is close to VP-tree. It is not faster in every configuration. Native-float `5bit` and `6bit` are exhaustive fallbacks rather than byte-addressed caches, which explains why their high-`K` curves differ substantially from their byte counterparts. These end-to-end measurements cannot assign the difference to graph walking, SIMD, or any other individual stage.

Variation is material at several points. For example, byte `egret` without diffusion at `K = 256` takes `80.58 ± 28.51 ms` for `5bit`, with a `71.41 ms` median and one `133.02 ms` sample; byte `snake` without diffusion at `K = 128` takes `74.10 ± 7.76 ms` for Delaunay. Every sample is retained, including these slow observations. Seven sequential samples and a fixed policy order do not establish statistical significance or a stable universal ranking.

The separate output probes produced byte-identical SIXEL for Delaunay and VP-tree in all 48 matched cases. This establishes equality for these fixtures and commands, including diffusion; it does not replace the [native-coordinate correctness tests](../delaunay.md#test-coverage). This study does not add perceptual quality scores, multithreaded or shared-instance results, or measurements of the float SIMD build.

## Interpretation

- The nine-policy quality study uses one natural image, and the Delaunay timing study adds a second photo and one screenshot. Neither characterizes every color distribution. Add gradients, saturated synthetic colors, alpha cases, and a broader image set before using the result to change a default.
- Mean Delta E00 includes lightness, chroma, and hue contributions but hides worst-case pixels. Mean chroma error omits hue and lightness.
- The quality test does not measure memory use, output size, or palette-index stability. Those require separate curves.
- The speed curve is end-to-end single-image latency, not an isolated lookup microbenchmark. Loader, palette construction, and SIXEL encoding add common work, while policy-dependent preparation remains part of the measurement.
- The Floyd--Steinberg control demonstrates one diffusion method. It does not predict ordered, arithmetic, or other error-diffusion methods.
- The focused shared-instance run fixes the thread limit rather than sweeping it. It does not characterize scaling, worker affinity, memory use, or parallel-band seam quality.
- The Metal run is specific to one GPU, input size, one-shot process model, and supported request shape. GPU/CPU crossover points can move with all four.

## Reproduction

Configure an Autotools build with libpng and the assessment tools, install Python Matplotlib, commit the implementation being measured, and run from a tracked-clean worktree:

```sh
tools/reproduce_lookup_policy_measurements.sh
```

This one command rebuilds `img2sixel` and `lsqa`, regenerates all eight quality sweeps and both speed sweeps, writes the plots, and records provenance in [`lookup-policy-run.json`](lookup-policy-run.json) and [`lookup-policy-fs-run.json`](lookup-policy-fs-run.json), and validates the complete Cartesian set of nine policies and configured palette sizes. It refuses tracked source changes so the recorded Git revision identifies the implementation that produced the results. Untracked files do not affect this check.

The driver uses [`tools/plot_quality_curve.py`](../../../../../tools/plot_quality_curve.py) for command execution, `lsqa` parsing, CSV output, and plotting. The default input is `images/snake.png`; the default performance budget is two warm-up rounds followed by nine measured rounds. `BUILD_DIR`, `PYTHON`, `MAKE`, `IMG2SIXEL_PATH`, `LSQA_PATH`, `LOOKUP_POLICY_WARMUPS`, and `LOOKUP_POLICY_RUNS` are explicit overrides. Supplying an output directory and input as the first two arguments supports exploratory runs without changing the checked-in artifacts.

On a macOS Metal host, reproduce the focused execution-mode comparisons with:

```sh
PYTHON=.venv/bin/python tools/reproduce_lookup_policy_acceleration.sh
```

The Python override is only an example; use an interpreter with Matplotlib. The runner rebuilds the project, measures both fixed matrices, forces Metal, checks CPU/Metal output equivalence, checks the required `S0`/`S1` output relation, writes [`lookup-policy-acceleration-run.json`](lookup-policy-acceleration-run.json), and invokes the common artifact validator. The default shared-instance thread limit is eight and can be changed with `LOOKUP_POLICY_SHARED_THREADS`; such a run is a different protocol and should not overwrite the checked-in results without updating their interpretation.

The Delaunay study uses a separate runner because its fixture set, representations, and seven-sample protocol differ from the nine-policy study. Configure and finish an Autotools build with libpng and LCMS2 in an out-of-tree directory, using `CFLAGS=-O3` to match the recorded build. Install Hyperfine, ImageMagick, and Python Matplotlib, and run the following from a clean source checkout containing Delaunay; `BUILD_DIR` must identify that checkout's completed build. Keep raw logs outside the source checkout, and run without other build or test jobs:

```sh
python3 tools/plot_lookup_policy_delaunay.py measure \
  docs/encoder/palette-application/lookup-policy/measurements \
  --source-dir "$PWD" --build-dir "$BUILD_DIR" \
  --raw-dir /tmp/lookup-policy-delaunay-raw
```

The runner refuses tracked or untracked source changes, verifies the source and executable/library hashes again after timing, requires every consumer class and paired SIXEL output to match, and then writes the JSON, CSV, and two figures. `--snapshot-kind=local-measurement` explicitly labels an isolated local snapshot, as used for the checked artifacts. A clean Git snapshot must already exist; the runner does not create commits. Timing JSON and trace files remain in the raw directory for inspection.

To regenerate only the Delaunay CSV and figures from the retained samples, then validate all checked artifacts without running benchmarks:

```sh
python3 tools/plot_lookup_policy_delaunay.py plot \
  docs/encoder/palette-application/lookup-policy/measurements
python3 tools/check_lookup_policy_measurements.py --require-delaunay \
  docs/encoder/palette-application/lookup-policy/measurements
```

The Delaunay validator requires the full 192-configuration matrix, seven successful samples per point, matching sample-derived statistics and CSV rows, the expected consumer classes, and all 48 equal output digests and lengths. The common validator also detects the Delaunay files automatically; `--require-delaunay` additionally rejects their complete absence. Plot regeneration requires Matplotlib; validation alone uses Python's standard library.

The quality wrapper regenerates the broad and focused controlled K-means comparisons and the broad and focused current-Heckbert compatibility comparisons. The CSV files store the full command template for every point. A manifest stores the source state and revision, input and executable SHA-256 digests, libtool payload digests, configure arguments, compiler flags, Python and Matplotlib versions, platform, and timing protocol. A quality rerun is comparable when those inputs and commands match. Compare speed only on the same otherwise-idle host with the same power and thermal conditions; the manifest makes host or build changes visible but cannot normalize them.

Rerun the complete command after changes to palette construction, lookup, working colorspaces, loading, SIXEL decoding, `lsqa`, compiler optimization, or the benchmark scripts. Review every CSV and plot diff together, then update the numeric tables and interpretation in this document in the same commit. Do not combine quality artifacts from one revision with speed artifacts from another revision.
