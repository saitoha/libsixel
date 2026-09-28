# SIXEL among contemporary image codecs

## Three independently useful ideas

SIXEL has an old wire syntax, but its ordinary image pipeline combines **color quantization**, **six-row color masks with run-length coding**, and **a terminal protocol that can be transmitted using only 7-bit bytes**. These are separate design choices. The terminal does not need to know whether the palette came from a classic median cut, a modern clustering solver, a perceptual color space, or a palette retained from an earlier frame.

Color quantization has continued to develop in the twenty-first century. For concrete examples, [Yinyang k-means (2015)](https://proceedings.mlr.press/v37/ding15.html) reduces redundant distance calculations in clustering, while [libimagequant's release history](https://github.com/ImageOptim/libimagequant/blob/main/CHANGELOG) records improvements to palette quality, fixed colors, remapping, dithering, and performance. The former is general clustering research, rather than a SIXEL benchmark; the latter is a production palette quantizer, rather than a new image syntax. Both illustrate why the age of a palette file format does not fix the quality or speed of its encoder. libsixel exposes these independently evolving stages through its [quantization models](../../encoder/palette-construction/quantization/README.md), [working color spaces](../../encoder/palette-application/working-colorspace/README.md), and [palette policies](../../overview.md).

The following comparison measures implementations and operating points, not a universal ranking of formats. It deliberately includes outcomes that favor the other codecs.

## Image formats and the terminal

Several of the formats compared on this page began close to terminals and telematic services. How each one relates to a terminal byte stream shows where SIXEL, the oldest of them, coincides with recent design directions and where it does not.

<picture>
  <source media="(max-width: 640px)" srcset="figures/codec-timeline-mobile.svg">
  <img src="figures/codec-timeline-wide.svg" alt="Timeline from 1970 to 2025, one event per row: ECMA-35 1971, ECMA-48 1976, VT240 1984, JPEG committee 1986, VT340 1987, GIF87a 1987, GIF89a specification 1990, JPEG T.81 1992, PNG 1.0 1996, JPEG 2000 2000, RLogin 2.9.0 2010, Gate One 1.0 2012, libsixel 2014 and QOI 2021." loading="lazy">
</picture>

| Format | Origin and intended use | How the end of an image is found | Bytes inside the image data |
| --- | --- | --- | --- |
| SIXEL | DEC printers, then graphics terminals such as the VT240 and VT340 | the ST control function that ends its DCS | printable 7-bit characters only |
| GIF (1987) | CompuServe's online service and its VIDTEX terminal software | a `;` trailer after length-prefixed data blocks | any byte |
| JPEG (1992) | photographic videotex, standardized in the telematic T series | the EOI marker; markers are recognizable without decoding | any byte except an unstuffed `0xFF` |
| PNG (1996) | a patent-free lossless replacement for GIF | the IEND chunk after length-prefixed chunks | any byte |
| JPEG 2000 (2000) | a successor to JPEG with scalable coding | the EOC marker in the codestream, but a JP2 box or last tile-part may extend to the end of the file or codestream | any byte, with marker values kept out of coded data |
| QOI (2021) | simple, fast lossless coding | a fixed eight-byte end marker; the header also gives the pixel count | any byte |

### Terminal control and SIXEL

[ECMA-35](https://ecma-international.org/publications-and-standards/standards/ecma-35/), which specifies the same code extension techniques as ISO/IEC 2022, was first published in December 1971, and [ECMA-48](https://ecma-international.org/publications-and-standards/standards/ecma-48/), which defines control functions such as DCS and ST, in March 1976. SIXEL is a Device Control String in that framework. Its image data consists of printable 7-bit characters, and ST ends it, so an ordinary ECMA-48 parser can carry it without a capture mode or a transport encoding. DEC used SIXEL for printers and then for screens, for example on the [VT240](https://terminals-wiki.org/wiki/index.php/DEC_VT240), introduced in January 1984, and the colour [VT340](https://terminals-wiki.org/wiki/index.php/DEC_VT340) of April 1987. See the [SIXEL format overview](../README.md) for the syntax.

### JPEG: born in telematic terminal standardization

The JPEG committee was created in November 1986 by experts linked to CCITT Study Group VIII, whose remit was terminal equipment for telematic services such as facsimile, Teletex and Videotex, and to ISO TC97 SC2/WG8, part of the committee for coded character sets whose scope also covered switching between character and graphic coding. Its motivating application was photographic videotex over ISDN, studied in the ESPRIT 563 PICA project. CCITT approved the result in 1992 as [T.81](https://www.w3.org/Graphics/JPEG/itu-t81.pdf), a recommendation in the T series for telematic terminals ([Sebestyén, 2020](https://www.itu.int/dms_pub/itu-s/opb/journal/S-JOURNAL-ICTS.V3I1-2020-13-PDF-E.pdf)).

T.81 assigns every marker the two bytes `0xFF` and a byte other than `0x00` or `0xFF`, and stuffs a `0x00` after any `0xFF` produced by entropy coding (clauses B.1.1.2 and B.1.1.5). A reader can therefore locate each part and the final EOI marker without decoding the image. The entropy-coded data can still contain every other byte value, including C0 and C1 control codes, so JPEG was not designed to pass through an ordinary terminal parser. A terminal must deliberately capture it. [Gate One](https://liftoff.github.io/GateOne/ReleaseNotes/index.html), a Python HTML5 terminal emulator whose 1.0 release in March 2012 displayed images written with `cat`, did this by carving a JPEG from its SOI marker to the next EOI, and a PNG through its IEND chunk ([`terminal.py`](https://github.com/liftoff/GateOne/blob/master/terminal/terminal.py)). A plain search for the first EOI ends too early when a marker segment embeds another JPEG, such as an EXIF thumbnail; the segments must be parsed by length.

### GIF: images for an online service's terminals

CompuServe published [GIF87a](https://www.w3.org/Graphics/GIF/spec-gif87.txt) on 15 June 1987 as an exchange and display mechanism for graphics on a variety of hardware, to be transferred with the B Protocol implemented in its VIDTEX terminal products. The [GIF89a specification](https://www.w3.org/Graphics/GIF/spec-gif89a.txt), dated 31 July 1990, adds an On-line Capabilities Dialogue: a host asks an interactive GIF decoder for its display capabilities with `ESC [ > 0 g`, a control sequence in the ECMA-48 form, and the decoder replies. A GIF stream consists of length-prefixed data sub-blocks and ends with a `;` trailer, while its LZW-compressed data are arbitrary bytes.

### PNG: lossless and patent-free

On 28 December 1994 CompuServe announced royalties for GIF software under Unisys's LZW patent; the first PNG draft followed on 4 January 1995, and PNG 1.0 became a W3C Recommendation on 1 October 1996 ([PNG history](http://www.libpng.org/pub/png/pnghist.html)). [RFC 2083](https://www.rfc-editor.org/rfc/rfc2083) describes PNG as a patent-free replacement for GIF that also covers truecolor and alpha, compressed losslessly with deflate, which combines LZ77 and Huffman coding. Its eight-byte signature is designed to detect damage by channels that are not 8-bit transparent: a non-ASCII first byte catches transfers that clear bit 7, CR-LF catches newline translation, and a control-Z stops display under MS-DOS. PNG treats text and terminal channels as hazards to detect rather than as a transport.

### JPEG 2000: markers remain, framing moves outward

[JPEG 2000](https://www.itu.int/rec/T-REC-T.800) keeps markers in its codestream, including an end-of-codestream marker, and keeps marker values out of coded data. Its framing, however, can defer to the surrounding structure: the last tile-part may declare a zero length and run until the end-of-codestream marker, and a box in the JP2 file format may declare a zero length meaning that it extends to the end of the file. A JP2 image therefore need not announce its own end within a longer byte stream, which makes it less suited than JPEG to being carved out of terminal output.

### QOI: speed from simplicity

Dominic Szablewski introduced [QOI](https://phoboslab.org/log/2021/11/qoi-fast-lossless-image-compression) on 24 November 2021 as a lossless format that uses neither Huffman coding nor a DCT; he reported sizes similar to PNG with 20 to 50 times faster compression and 3 to 4 times faster decompression, single-threaded and without SIMD. The [specification](https://qoiformat.org/qoi-specification.pdf) (version 1.0, 5 January 2022) encodes each pixel as a run, an index into a 64-entry array of previously seen colors, a difference from the previous pixel, or a literal value. Decoding therefore proceeds strictly in order; QOI's advantage is the absence of an entropy coder, not parallel structure.

### Where SIXEL fits

SIXEL has no entropy coding, but not as an optimization. It was first used to drive DEC dot-matrix printers, where each data character directly controlled six pins of the print head, and graphics terminals then interpreted it in their own hardware ([Sixel](https://en.wikipedia.org/wiki/Sixel)). A representation that such devices could act on character by character, over a 7-bit printable channel, is the likely reason it has no statistical coder. Its resemblance to QOI is a coincidence of simplicity, not a shared design goal.

That simplicity does not match every property of current processors. Repeat counts and color register numbers are variable-length decimal text after `!` and `#`, as described in the [format overview](../README.md#body-control-functions). Their parsing depends on each digit and on where a number ends, so decoding proceeds character by character with data-dependent branches that fixed-width SIMD lanes handle poorly, and encoding must format each number in decimal. Byte-aligned binary fields, such as QOI's chunks, avoid that work.

Band parallelism was not designed in either. The `-` control separates six-row bands, and a later technique uses it: libsixel's decoder splits a stream at arbitrary byte offsets, advances each part to the next `-`, and reconstructs the palette and cursor state needed there ([decoder threading](../../threading/decoder.md)). That scan is an extra cost before painting, and a register redefined after use or overlapping paint requires a sequential fallback, as the next section notes. On the encoder side, libsixel parallelizes palette application and band serialization as an implementation choice rather than because the format provides for it. The [threading overview](../../threading/overview.md) defines SIXEL bands, work bands and decoder byte spans; [encoder threading](../../threading/encoder.md) and [decoder threading](../../threading/decoder.md) describe the implementations, and the [threading documentation](../../threading/README.md) indexes their measurements.

What remains distinctive is the transport. SIXEL's bytes are printable characters inside a control string, so it travels over the terminal channel itself, whereas GIF relied on file transfer and JPEG on terminal-side capture. Its costs, a palette limit and larger streams, are measured in the rest of this page.

## Compression and concurrency are different properties

| Format / mode | Image representation and compression | Parallelism relevant to a single image | Native 7-bit terminal image protocol? |
| --- | --- | --- | --- |
| Ordinary SIXEL | Quantized palette, six-row color masks, decimal run lengths | Palette application and band serialization can run concurrently; decoder scans establish state before independent painting | Yes, with the 7-bit DCS/ST envelope and a SIXEL-capable terminal |
| GIF | Indexed palette and LZW dictionary coding | Palette construction can be parallelized; LZW decoding has dictionary dependencies | No |
| PNG | RGB/RGBA, grayscale, or indexed pixels; filtering and DEFLATE | Filtering and implementation-specific work can be parallelized; DEFLATE history and row-filter dependencies constrain straightforward splitting | No |
| QOI | RGB/RGBA runs, a 64-entry array of recent colors, differences and literals; no entropy coding | Sequential: every pixel depends on the previous pixel and the color array | No |
| Baseline JPEG used here | Chroma subsampling, block DCT, coefficient quantization and Huffman coding | Block transforms offer parallel work; restart intervals can provide independent entropy segments | No |
| Lossy WebP | Prediction, block transforms and entropy coding | libwebp provides encoder and decoder multithreading switches; the amount of parallel work depends on the path | No |
| Lossless WebP | Reversible transforms, backward references and entropy coding | Different dependencies from lossy WebP; enabling the switch does not promise the same speedup | No |

GIF uses [LZW](https://giflib.sourceforge.net/whatsinagif/lzw_image_data.html), not Huffman coding. PNG's [DEFLATE](https://www.w3.org/TR/PNG-Compression.html) combines LZ77 with Huffman coding. Huffman coding itself is not a proof that a format cannot be parallelized: JPEG [restart markers](https://datatracker.ietf.org/doc/html/rfc2435#section-4.4) provide resynchronization boundaries. Parallel performance depends on independently decodable partitions, state reconstruction, implementation, and memory traffic, as well as compression work.

SIXEL's simpler run-length expansion can therefore be faster than a more compact codec on an appropriate workload, particularly when independent bands are processed concurrently. It is a possibility to measure, not a general claim that entropy-coded formats are slow. SIXEL must still construct or reuse a palette, assign pixels, scan commands, establish palette/cursor state, and move its often larger byte stream. See [encoder threading](../../threading/encoder.md) and [decoder threading](../../threading/decoder.md) for the actual implementation. Arbitrary SIXEL streams are not independently decodable at every byte offset: register redefinition or overlapping paint can require fallback.

## What is measured

The photographic workload is a corpus of 100 photographs, the `photo/original/*.jpg` files of the libsixel-dataset repository. Each is rotated upright by its EXIF orientation, converted from an embedded ICC profile to sRGB when it has one (67 photographs, six of them Adobe RGB), and fitted inside 1920 × 1080 with Lanczos resampling while keeping its aspect ratio; the results range from 735 × 1080 to 1920 × 1080. The profile is dropped after conversion, so every codec and the quality reference receive the same untagged sRGB samples. The photographs are not stored with the results: the [raw results](figures/results.json) record the SHA-256 of each source file and of each prepared input. A second workload is a deterministic 768 × 512 flat-color diagram with text, stored as [diagram.png](figures/diagram.png). No alpha, animation, metadata preservation, terminal painting, display upload, or hardware codec work is measured.

Each encode starts with pixels in memory and ends with complete image bytes in memory. Each decode starts with bytes in memory and ends with a loaded RGB image, except for the explicitly labeled retained-index generation path. Timings include the Python/native adapters, input/output copies, allocation and cleanup; they exclude disk I/O, process startup, LSQA, Base64 conversion and terminal rendering. They are application-call latencies, not isolated inner-loop throughput. Different wrapper overheads remain a limitation, especially for sub-millisecond JPEG calls. SIXEL decoding uses `sixel_decode_pixels` directly; no downstream PNG writer is included.

| Implementation | Settings |
| --- | --- |
| libsixel, default | Archived committed source; optimized C99 build; 256 colors; `--precision=8bit`, `--gpu-policy=off`, `-E fast`; default diffusion; borrowed RGB8 or PAL8 frame API |
| libsixel, quality | The same build and API with the equivalent of `img2sixel -Qk -Fw -Wc -Xc -dl`: K-means palette, Ward final merge, CIELAB clustering and working color spaces, LSO2 diffusion; `--gpu-policy=off` and library defaults otherwise. The thread budget follows the 1/2/4/8 axis rather than a fixed `-=` value |
| Pillow JPEG / libjpeg-turbo | Quality 60, 80 and 95; 4:2:0; no optimized Huffman table pass |
| Pillow GIF | 256-color median-cut quantization for RGB input; existing indices for retained-index input; `optimize=False` |
| Pillow PNG / zlib | RGB PNG; compression level 6 |
| QOI | The [`qoi`](https://pypi.org/project/qoi/) Python package, which wraps the reference C implementation; RGB arrays |
| libwebp | Quality 60, 80 and 95, method 4; separate lossless quality/effort 80, method 4; native API multithreading switch |

Seven samples follow two warm-ups per cell. For each photograph, the charts use the median of its seven samples; latency panels show the distribution of those per-photograph medians as box plots, and quality, size, scaling and generation panels show their means. The diagram charts use the median and interquartile range of the seven samples. Timed repetitions within a generation start from the same preceding-generation image; only the final result advances the chain. The generation experiment runs on every fifth photograph by name (20 photographs) and on the diagram. Quality is measured with the repository's `lsqa`, against the prepared input for every generation, using MS-SSIM and mean Delta E00; identical decoded pixels are evaluated once. PNG, QOI and lossless WebP are also checked for exact RGB equality; perceptual scores do not substitute for this check. Values below the project's usual 0.98/0.95 quality gates are reported as failed quality operating points, not accepted regression thresholds.

The 1/2/4/8 axis is a **configured worker budget**, not CPU affinity or reserved cores. SIXEL receives that count. The [WebP API](https://developers.google.com/speed/webp/docs/api) receives `thread_level=1` / `use_threads=1` for every budget above one, because these are Boolean controls rather than exact counts. The measured Pillow JPEG/GIF/PNG paths remain serial for a single image. Thus their additional-budget points do not imply use of 2/4/8 workers. All runs are sequential; there is no multi-image throughput benchmark. Other host activity and scheduling can affect the results.

Decoder scaling always uses the same bytes produced by the one-thread encoder. Encoder scaling also records quality and size at each budget: parallel palette/diffusion choices can change its result. These graphs must not present a changed-quality encoder as a pure same-output speedup. Equal numerical quality settings in JPEG and WebP likewise do not mean equal quality; consult the measured quality/size points.

## Transport size: three bytes become four

For a binary image of `n` bytes, standard padded Base64 uses `4 * ceil(n / 3)` bytes, before any surrounding terminal graphics command, chunk headers or line breaks. The comparison calls this **7-bit transport payload**, not total protocol traffic. For SIXEL it uses the native stream size, including its DCS, raster/palette commands and ST. Every generated SIXEL stream is checked to use only bytes below 128 and to have a 7-bit envelope.

The roughly 33% expansion changes the comparison but does not guarantee a SIXEL size advantage. JPEG and WebP can remain substantially smaller after Base64. Furthermore, Base64 alone does not make JPEG/GIF/PNG/WebP render on a terminal: a receiving protocol and implementation are still required. A 7-bit-clean connection also does not imply SIXEL support. Multiplexer wrapping and transport-specific escaping are outside this model.

## Observed results on the recorded host

The checked-in run used an Apple M3 Max (14 logical CPUs), macOS 26.5.1, Apple Clang 21 with `-O3`, libsixel revision `08ca59d8840a3f2e25f9d7ee303819256b58cdf1`, Pillow 12.3.0 with libjpeg-turbo 3.1.4.1 and zlib-ng reporting 1.3.1.zlib-ng, `qoi` 0.7.2, and libwebp 1.6.0. Plotting used matplotlib 3.10.3. The revision was the `develop` branch when the run started; that branch was later rebased, and its library sources at `9bdd515c89ec1327c68e2da16108741e477c4111` are identical. The source revision identifies the isolated codec build, not unrelated work in the shared checkout.

| Observation over the 100 photographs | Mean of per-photograph medians or scores |
| --- | --- |
| Default SIXEL encode, budget 1 → 8 | 470.6 → 101.3 ms (median per-photograph speedup 5.4×); mean MS-SSIM 0.9805 → 0.9825 |
| Quality SIXEL encode, budget 1 → 8 | 631.6 → 230.5 ms; mean MS-SSIM 0.9968 → 0.9952, mean Delta E00 2.07 at budget 1 |
| Default SIXEL decode, identical bytes, budget 1 → 8 | 24.7 → 14.7 ms (median per-photograph speedup 1.9×) |
| PNG / QOI, budget 1 | encode 77.9 / 9.7 ms, decode 18.8 / 7.6 ms; both lossless |
| JPEG quality 80, budget 1 | 2.7 ms encode, 3.0 ms decode; mean MS-SSIM 0.9972 |
| Transport payload per photograph | default SIXEL 2,348 KiB, quality SIXEL 2,036 KiB, PNG 3,038 KiB, QOI 3,414 KiB, JPEG 80 306 KiB, WebP 80 231 KiB |
| Retained-index encode, generation 1 → 2 (20 photographs) | SIXEL 521.4 → 116.8 ms, GIF 165.5 → 15.2 ms; generations 2–8 preserve generation-one RGB exactly |

Eight-budget SIXEL decoding is faster than this RGB PNG decode for 89 of the 100 photographs, and on average faster than quality-95 and lossless WebP, but slower than QOI, JPEG, GIF and quality-60/80 WebP decoding. Default SIXEL's transport payload is smaller than PNG's for 94 photographs and smaller than QOI's for 97, but it is lossy and much larger than JPEG and lossy WebP. On the diagram, eight-budget default SIXEL encodes in about 6.6 ms versus 12.0 ms for lossy WebP quality 80, while PNG, QOI and JPEG remain faster. These are workload-dependent tradeoffs, not a Huffman-versus-RLE law or a matched-quality victory.

The default settings have a real limitation. Five photographs score below 0.95 MS-SSIM with default SIXEL, the lowest 0.567 on `photo-064`, although their mean Delta E00 of 1.1–3.1 is no worse than the corpus mean of 3.49. In `photo-064`, a black background covers about 81% of the image; the default palette has no exact black, and the background becomes a uniform near-black value. MS-SSIM's luminance term penalizes that offset strongly where the reference mean is near zero. The quality settings keep every photograph at or above 0.95. Such points are unsuitable when that quality floor is required, and they also pull down the default mean.

## Photographic workload

<picture>
  <source media="(max-width: 640px)" srcset="figures/photo-profile-mobile.svg">
  <img alt="Photographic workload: encoder and RGB decoder latency, measured quality against Base64-adjusted payload size, and native versus transport bytes for SIXEL, GIF, PNG, QOI, JPEG and WebP over 100 photographs." src="figures/photo-profile-wide.svg">
</picture>

The latency boxes compare all sampled codec configurations across the corpus; the quality plot shows why file size and latency cannot be interpreted without distortion, with each photograph as a small mark and each configuration's mean as a numbered mark. Points far below the others are counted in the panel rather than stretching its axis. The native/transport panel uses representative quality-80 JPEG/WebP rows, both SIXEL settings, GIF, PNG, QOI and lossless WebP.

<picture>
  <source media="(max-width: 640px)" srcset="figures/photo-scaling-mobile.svg">
  <img alt="Photo under budgets one, two, four and eight: encode latency, fixed-stream RGB decode latency, encoder MS-SSIM and encoder size; WebP uses a Boolean parallel switch and Pillow codecs stay serial." src="figures/photo-scaling-wide.svg">
</picture>

## Repeated encoding is a state contract

The generation experiment decodes GIF/SIXEL into its index plane and palette and supplies that representation to the next encoder. JPEG, PNG and WebP use their decoded RGB pixels. The first generation always starts with the same original RGB image. Palette construction and nearest-color assignment can then be bypassed from generation two onward for GIF/SIXEL. This is reuse of the decoded representation, not reuse of previously compressed bytes or a warmed encoder object; each SIXEL encode creates a fresh encoder.

This indexed path reflects ordinary `img2sixel` SIXEL-to-SIXEL conversion. The [encoder](../../../src/encoder.c) enables palette retention for ordinary input without resizing or a color override, and the SIXEL loader in [loader-builtin.c](../../../src/loader-builtin.c) returns PAL8 pixels with their palette when the requested palette capacity suffices. The command sequence is:

```sh
img2sixel --threads=1 -o generation1.six photo.png
img2sixel --threads=1 -o generation2.six generation1.six
img2sixel --threads=1 -o generation3.six generation2.six
```

A separate CLI verification using the recorded libsixel source build repeated this operation through generation eight for the 20 generation photographs and the diagram. Every generation's decoded RGB pixels matched generation one exactly. This verifies the actual file-loader/encoder path; the graphed timings remain the in-memory API measurements described above and do not include CLI startup or file I/O.

The generation graphs therefore retain one native representation path per codec. SIXEL/GIF preserve first-generation pixels in all measured generations, including the initial quantization error. PNG, QOI and lossless WebP preserve the original RGB pixels from generation one without needing a palette; indexed PNG could also reuse palette state, although that mode is not measured here. Lossy JPEG/WebP can accumulate distortion, but the amount depends on the image and settings; a sequence can approach a stable point rather than losing the same amount at every step.

<picture>
  <source media="(max-width: 640px)" srcset="figures/photo-generations-mobile.svg">
  <img alt="Eight photographic encode/decode generations: MS-SSIM and Delta E00 against the original, encoding latency, and decoding latency. GIF/SIXEL retain their decoded palettes and indices between generations." src="figures/photo-generations-wide.svg">
</picture>

Indexed decode times have a different output contract from RGB decode times and must not be ranked as interchangeable decoder performance. All generation quality measurements expand indices to RGB outside the timed region.

## Diagram workload

Photographs hide the favorable long runs in flat graphics. The diagram keeps that workload separate instead of averaging it into the corpus.


<picture>
  <source media="(max-width: 640px)" srcset="figures/diagram-profile-mobile.svg">
  <img alt="Diagram profile: the same codec settings and measurement boundaries as the photographic workload; full numerical values are in results.json." src="figures/diagram-profile-wide.svg">
</picture>

<picture>
  <source media="(max-width: 640px)" srcset="figures/diagram-scaling-mobile.svg">
  <img alt="Diagram scaling: the same codec settings and measurement boundaries as the photographic workload; full numerical values are in results.json." src="figures/diagram-scaling-wide.svg">
</picture>

<picture>
  <source media="(max-width: 640px)" srcset="figures/diagram-generations-mobile.svg">
  <img alt="Diagram generations: the same codec settings and measurement boundaries as the photographic workload; full numerical values are in results.json." src="figures/diagram-generations-wide.svg">
</picture>

## Reproduction and provenance

The [measurement adapter](../../../tools/format-comparison/codecs.c), [driver](../../../tools/format-comparison/measure.py), [result validator](../../../tools/format-comparison/check.py), and [plotter](../../../tools/format-comparison/plot.py) are maintained with this guide. The driver verifies that the adapter and decoder resolve the same libsixel library. This matters on macOS, where an absolute dylib install name can override a build-directory rpath.

Install Python dependencies (`Pillow`, `numpy`, `qoi`, `matplotlib`) in a dedicated environment, and provide a C compiler, pkg-config, libwebp development files, the project's build prerequisites and a directory of source photographs such as `photo/original` of the libsixel-dataset repository. Then run from the repository root:

```sh
PYTHON=/path/to/venv/bin/python PHOTOS=/path/to/libsixel-dataset/photo/original \
    tools/reproduce_format_comparison.sh
```

The recorded run took about three hours and twenty minutes, most of it spent in LSQA on the Full HD outputs. Keep the host awake and on power for the whole run: a sleep inside a timed region inflates that sample.

The wrapper archives a committed revision into an isolated temporary source directory and builds it with `CFLAGS=-O3`. It never benchmarks the shared dirty worktree. Set `REVISION` to reproduce a particular recorded implementation; a branch name is resolved to its commit before the run, and that commit is recorded. The photographs are identified by the SHA-256 of each source file and each prepared input, so a different photo directory is detected when results are compared. The saved records retain every timing sample. The metadata records the full source revision, compiler, CPU, runtime and codec versions, build configuration, adapter/library digests, script digests, the input size and digest of every image, the digest of every source photograph, the ICC profile each photograph was converted from, and the photographs used in the generation experiment. Timing and output records include all seven samples, stream/pixel hashes, quality, native bytes, transport bytes and generation equality assertions. The source snapshot is kept in the temporary study directory for inspection; `STUDY_DIR` can select that location.

To validate the saved data and regenerate figures without rerunning measurements:

```sh
python tools/format-comparison/check.py docs/sixel-format/comparison/figures
python tools/format-comparison/plot.py docs/sixel-format/comparison/figures
```

Pass `--check` to the plotter to verify byte-for-byte figure freshness without writing files. The SVGs have deterministic identifiers and no timestamp; use the same matplotlib version for byte-identical plot regeneration. Wide figures use a two-column layout and mobile figures stack the same four panels. The diagram input and raw JSON remain available when the host Markdown renderer does not honor `<picture>` media selection.
