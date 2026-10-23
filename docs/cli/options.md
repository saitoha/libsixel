# Command-Line Options at a Glance

## Scope

This page lists every option of the `img2sixel` and `sixel2png` converters with a one-line summary and a link to the document that explains it in depth. It is a map for choosing options, not a specification: the manual pages [`img2sixel(1)`](../../converters/img2sixel.1) and [`sixel2png(1)`](../../converters/sixel2png.1) define the exact syntax, accepted values, and defaults.

The **Level** column marks how much background an option needs:

- **basic**: usable without knowledge of color science or the encoder's internals;
- **advanced**: tunes a pipeline stage; read the linked document, and the [concepts](../README.md#concepts-and-theory) it relies on, before changing it from the default.

Defaults are chosen so that most users never need an advanced option. The [encoding pipeline](../encoder/pipeline/README.md) shows where each stage fits.

## `img2sixel`

### Input, output, and terminal

| Option | Level | Summary | Details |
| --- | --- | --- | --- |
| `-o`, `--outfile` | basic | Write output to a file instead of standard output. | [`img2sixel(1)`](../../converters/img2sixel.1) |
| `-y`, `--clipboard-policy` | basic | Read input from or write output to the clipboard. | [Clipboard](clipboard.md) |
| `-7`, `--7bit-mode` | basic | Use 7-bit control sequences (default). | [SIXEL format](../sixel-format/README.md) |
| `-8`, `--8bit-mode` | basic | Use 8-bit C1 control characters. | [SIXEL format](../sixel-format/README.md) |
| `-R`, `--gri-limit` | basic | Limit repeat counts to 255 for older receivers. | [SIXEL format](../sixel-format/README.md) |
| `-z`, `--terminal-policy` | basic | Control cursor hiding during animated terminal output. | [Animation playback](../encoder/animation.md) |
| `-L`, `--loaders` | advanced | Choose and order the image loaders. | [Image loader architecture](../loader/README.md) |
| `-k`, `--insecure` | basic | Skip HTTPS certificate verification for URL input. | [Network access](../loader/network-access.md) |
| `-#`, `--cms-engine` | advanced | Select the color-management engine used by loaders. | [Loader color management](../loader/color-management/README.md) |

### Size and region

| Option | Level | Summary | Details |
| --- | --- | --- | --- |
| `-w`, `--width` | basic | Resize to a width in pixels or percent; `auto` keeps the aspect ratio. | [Crop and resize](../encoder/preprocessing/crop-resize/README.md) |
| `-h`, `--height` | basic | Resize to a height in pixels or percent; `auto` keeps the aspect ratio. | [Crop and resize](../encoder/preprocessing/crop-resize/README.md) |
| `-c`, `--crop` | basic | Crop the source to a geometry before resizing. | [Crop and resize](../encoder/preprocessing/crop-resize/README.md) |
| `-r`, `--resampling` | advanced | Choose the resampling filter for resizing. | [Resampling](../encoder/preprocessing/resampling/README.md) |

### Colors and palettes

| Option | Level | Summary | Details |
| --- | --- | --- | --- |
| `-p`, `--colors` | basic | Number of palette colors (default 256). | [Palette quantization](../encoder/palette-construction/quantization/README.md) |
| `-b`, `--builtin-palette` | basic | Use a built-in palette, such as the xterm 16- or 256-color map or a VT340 map. | [Built-in palettes](../encoder/palette-construction/fixed-palettes.md) |
| `-e`, `--monochrome` | basic | Output a monochrome image. | [Built-in palettes](../encoder/palette-construction/fixed-palettes.md) |
| `-i`, `--invert` | basic | Assume a white terminal background for monochrome output. | [Built-in palettes](../encoder/palette-construction/fixed-palettes.md) |
| `-m`, `--mapfile` | basic | Use the colors of a given image or palette file. | [External palettes](../encoder/palette-construction/external-palettes.md) |
| `-M`, `--mapfile-output` | basic | Save the generated palette to a file. | [External palettes](../encoder/palette-construction/external-palettes.md) |
| `-I`, `--high-color` | advanced | Emit high-color output instead of a 256-color palette. | [High-color output](../encoder/output/high-color/README.md) |
| `-Q`, `--quantize-model` | advanced | Select the palette solver. | [Palette quantization](../encoder/palette-construction/quantization/README.md) |
| `-q`, `--quality` | advanced | Select the speed and quality profile of palette construction. | [Palette quantization](../encoder/palette-construction/quantization/README.md) |
| `-f`, `--find-largest` | advanced | Median-cut box selection rule. | [Palette quantization](../encoder/palette-construction/quantization/README.md) |
| `-s`, `--select-color` | advanced | Median-cut representative color rule. | [Palette quantization](../encoder/palette-construction/quantization/README.md) |
| `-4`, `--sampling-policy` | advanced | Choose which pixels feed palette construction; `auto` uses `adaptive-grid` independently of thread count. | [Palette construction pipeline](../encoder/palette-construction/pipeline/README.md) |
| `-5`, `--binning-policy` | advanced | Choose how sampled colors are weighted for the solver. | [Palette construction pipeline](../encoder/palette-construction/pipeline/README.md) |
| `-X`, `--clustering-colorspace` | advanced | Color space in which the palette is clustered. | [Clustering color space](../encoder/palette-construction/clustering-colorspace/README.md) |
| `-F`, `--merge-policy` | advanced | Final palette merging after the solver. | [Merge policy](../encoder/palette-construction/merge-policy/README.md) |
| `-a`, `--cover-policy` | advanced | Repair gamut reachability with anchored palette entries. | [Cover policy](../encoder/palette-construction/cover-policy/README.md) |
| `-_`, `--snap-policy` | advanced | Move palette channels toward exactly representable SIXEL levels. | [Snap policy](../encoder/palette-construction/snap-policy/README.md) |

### Dithering and palette application

| Option | Level | Summary | Details |
| --- | --- | --- | --- |
| `-d`, `--diffusion` | advanced | Choose the dithering method. | [Dithering](../encoder/palette-application/dithering/README.md) |
| `-~`, `--lookup-policy` | advanced | Choose the nearest-palette-color search. | [Lookup policy](../encoder/palette-application/lookup-policy/README.md) |
| `-W`, `--working-colorspace` | advanced | Color space in which palette colors are chosen and errors diffused. | [Working color space](../encoder/palette-application/working-colorspace/README.md) |
| `-0`, `--clamp-policy` | advanced | Limit error-diffusion feedback. | [Clamp policy](../encoder/palette-application/clamp-policy.md) |

### SIXEL output

| Option | Level | Summary | Details |
| --- | --- | --- | --- |
| `-E`, `--encode-policy` | advanced | Favor encoding speed or stream size. | [Encoding policy](../encoder/output/encode-policy/README.md) |
| `-U`, `--output-colorspace` | advanced | Color space of the emitted palette values, such as Display P3. | [Output color space](../encoder/output/output-colorspace.md) |
| `-t`, `--palette-type` | advanced | Write palette definitions as RGB or HLS. | [`img2sixel(1)`](../../converters/img2sixel.1) |
| `-O`, `--ormode` | advanced | Emit OR-mode SIXEL. | [OR-mode output](../encoder/output/or-mode/README.md) |
| `-Z`, `--update-policy` | advanced | Emit full images or retained-plane delta updates. | [Delta encoding](../encoder/output/delta-encoding.md) |
| `-u`, `--use-macro` | advanced | Store the image in a terminal macro. | [Terminal macros](../encoder/output/terminal-macros.md) |
| `-n`, `--macro-number` | advanced | Choose the terminal macro number. | [Terminal macros](../encoder/output/terminal-macros.md) |
| `-@`, `--drcs` | advanced | Emit DRCS-SIXEL glyph definitions. | [DRCS-SIXEL output](../encoder/output/drcs.md) |

### Transparency and background

| Option | Level | Summary | Details |
| --- | --- | --- | --- |
| `-B`, `--bgcolor` | basic | Background color for compositing transparent input. | [Background policy](../loader/background-policy.md) |
| `-N`, `--background-policy` | advanced | Prefer the file's background or an explicit one (`-B`, environment, or terminal reply). | [Background policy](../loader/background-policy.md) |
| `-A`, `--alpha-policy` | advanced | Composite alpha or keep transparent pixels unpainted. | [Alpha policy](../loader/alpha-policy/README.md) |
| `-+`, `--transparent-offset` | advanced | Add a transparent left and top offset. | [Alpha policy](../loader/alpha-policy/README.md) |

### Animation

| Option | Level | Summary | Details |
| --- | --- | --- | --- |
| `-S`, `--static` | basic | Show only one frame of an animation. | [Animation playback](../encoder/animation.md) |
| `-l`, `--loop-control` | basic | Follow the file's loop setting, always loop, or never loop. | [Animation playback](../encoder/animation.md) |
| `-g`, `--ignore-delay` | basic | Play frames without their delays. | [Animation playback](../encoder/animation.md) |
| `-T`, `--start-frame` | basic | Start from a given frame. | [Animation playback](../encoder/animation.md) |

### Execution and diagnostics

| Option | Level | Summary | Details |
| --- | --- | --- | --- |
| `-v`, `--verbose` | basic | Print diagnostic information and the processing plan. | [Diagnostics](diagnostics.md) |
| `-=`, `--threading-policy` | advanced | Thread budget (`auto`, `serial` or a count), per-stage limits and frame concurrency. | [Encoder threading](../threading/encoder.md) |
| `-.`, `--precision-policy` | advanced | Precision policy: `auto`, `8bit` palette application, or `float32`. | [Encoder working precision](../encoder/precision/README.md) |
| `-G`, `--gpu-policy` | advanced | Use the GPU for palette application where supported. | [Metal palette application](../encoder/palette-application/lookup-policy/measurements/README.md#metal-paletteapply-performance) |
| `-j`, `--runtime-policy` | advanced | Process-wide SIMD and scheduling tuning. | [Runtime execution policy](runtime-policy.md) |
| `-x`, `--diagnostics` | advanced | Diagnostic presentation and trace topics. | [Diagnostics](diagnostics.md) |
| `-J`, `--log-path` | advanced | Write a JSON timeline log. | [Diagnostics](diagnostics.md) |

### Environment, completion, and information

| Option | Level | Summary | Details |
| --- | --- | --- | --- |
| `-%`, `--env` | advanced | Set an environment variable for this conversion. | [Suboption architecture](suboptions.md) |
| `-K`, `--completion-policy` | basic | Select the shell-completion source and install paths. | [`img2sixel(1)`](../../converters/img2sixel.1) |
| `-1`, `--show-completion` | basic | Print a shell-completion script. | [`img2sixel(1)`](../../converters/img2sixel.1) |
| `-2`, `--install-completion` | basic | Install shell-completion scripts. | [`img2sixel(1)`](../../converters/img2sixel.1) |
| `-3`, `--uninstall-completion` | basic | Remove installed shell-completion scripts. | [`img2sixel(1)`](../../converters/img2sixel.1) |
| `-V`, `--version` | basic | Show version and license information. | [`img2sixel(1)`](../../converters/img2sixel.1) |
| `-H`, `--help` | basic | Show help and the available loaders. | [`img2sixel(1)`](../../converters/img2sixel.1) |

### Deprecated

`-C` (`--complexion-score`), `-P` (`--penetrate`), and `-D` (`--pipe-mode`) are accepted for compatibility; see [`img2sixel(1)`](../../converters/img2sixel.1).

## `sixel2png`

| Option | Level | Summary | Details |
| --- | --- | --- | --- |
| `-i`, `--input` | basic | Read SIXEL from a file instead of standard input. | [`sixel2png(1)`](../../converters/sixel2png.1) |
| `-o`, `--output` | basic | Write PNG to a file instead of standard output. | [`sixel2png(1)`](../../converters/sixel2png.1) |
| `-y`, `--clipboard-policy` | basic | Read from or write to the clipboard. | [Clipboard](clipboard.md) |
| `-T`, `--input-colorspace` | advanced | Tag decoded RGB as sRGB or Display P3 in the PNG, without converting it. | [PNG writer](../writers/png/README.md) |
| `-D`, `--direct` | advanced | Decode to an RGBA surface; unpainted pixels stay transparent. | [Decoding pipeline](../decoder/pipeline/README.md) |
| `-d`, `--dequantize` | advanced | Reconstruct smooth tones from a dithered image. | [Dequantization](../decoder/dequantization/README.md) |
| `-S`, `--similarity` | advanced | Dequantization similarity bias. | [Dequantization](../decoder/dequantization/README.md) |
| `-s`, `--size` | advanced | Dequantization neighborhood size. | [Dequantization](../decoder/dequantization/README.md) |
| `-e`, `--edge` | advanced | Dequantization edge bias. | [Dequantization](../decoder/dequantization/README.md) |
| `-=`, `--threading-policy` | advanced | Thread budget (`auto`, `serial` or a count) and the `decode` worker limit. | [Decoder threading](../threading/decoder.md) |
| `-G`, `--gpu-policy` | advanced | Use a supported GPU for decoding. | [Dequantization](../decoder/dequantization/README.md) |
| `-j`, `--runtime-policy` | advanced | Process-wide SIMD and scheduling tuning. | [Runtime execution policy](runtime-policy.md) |
| `-x`, `--diagnostics` | advanced | Diagnostic presentation and trace topics. | [Diagnostics](diagnostics.md) |
| `-J`, `--log-path` | advanced | Write a JSON timeline log. | [Diagnostics](diagnostics.md) |
| `-%`, `--env` | advanced | Set an environment variable for this conversion. | [Suboption architecture](suboptions.md) |
| `-V`, `--version` | basic | Show version and license information. | [`sixel2png(1)`](../../converters/sixel2png.1) |
| `-H`, `--help` | basic | Show help. | [`sixel2png(1)`](../../converters/sixel2png.1) |
