# PNG Writer

This document defines how libsixel turns decoded or caller-supplied pixels into PNG data. It covers the compile-time libpng and builtin backends, the indexed and direct 8-bit output contracts, and the special `img2sixel` path that generates SIXEL before recreating a PNG for quality inspection.

The owning implementation is [`src/writer.c`](../../src/writer.c). [`src/decoder.c`](../../src/decoder.c) selects the pixel representation passed to the writer, while [`src/encoder.c`](../../src/encoder.c) owns the post-SIXEL snapshot path.

## Entry points and data flow

The public entry point is `sixel_helper_write_image_file()`. `SIXEL_FORMAT_PNG` is the only implemented output format accepted by this helper; the other enumerated formats return `SIXEL_NOT_IMPLEMENTED`. A null allocator selects a temporary default allocator. The filename `-` selects standard output, which is placed in binary mode where the platform requires it; any other filename is opened in binary write mode. The writer explicitly flushes the stream and closes only files that it opened.

There are two command paths into the writer:

```text
sixel2png
  -> parse and render SIXEL
  -> choose PAL8, RGB888, or RGBA8888 output
  -> sixel_helper_write_image_file(..., SIXEL_FORMAT_PNG)
  -> selected PNG backend

img2sixel with -o output.png, -o png:<path>, or -o png:-
  -> load source image
  -> resize, quantize, apply the selected palette, and dither
  -> generate the normal SIXEL byte stream into a temporary file
  -> decode that generated SIXEL with the standard decoder defaults
  -> sixel_helper_write_image_file(..., SIXEL_FORMAT_PNG)
  -> selected PNG backend
```

The second path is intentionally a post-SIXEL snapshot, not serialization of the input image or a pre-SIXEL encoder buffer. For an ordinary stable-palette SIXEL stream, the decoder returns the painted palette indices and palette, so the PNG exposes the quantized and dithered result that the generated SIXEL represents. This is the path intended for encoder quality inspection. The same command options must therefore affect both normal SIXEL output and PNG snapshot output before the paths diverge at the final destination.

The decoder selects the writer representation as follows:

| Decoder path | Writer input | PNG class |
| --- | --- | --- |
| Default stable-palette decode, with no dequantizer and no resizing promotion | `PAL8` plus palette | Indexed 8-bit |
| Palette decode followed by `k_undither`, `selective_blur`, or another RGB-producing reconstruction | `RGB888` | Direct RGB8 |
| `sixel2png -D`, including direct output after dequantization | `RGBA8888` | Direct RGBA8 |
| Resize or another frame operation that promotes the decoded representation | Promoted frame format, normally `RGB888` or `RGBA8888` | Matching direct 8-bit class |

High-color SIXEL is a boundary of the indexed snapshot contract. High-color output can redefine a color register after earlier pixels have used it, so no single indexed plane plus final palette can preserve every painted color. The current `img2sixel` PNG snapshot invokes the decoder's default indexed path and must not be treated as an exact high-color reference. Use direct decoder output when validating a register-redefining SIXEL stream. Animation also passes through the decoder's existing output behavior; this document does not define the PNG target as an APNG writer.

## Pixel input normalization

The writer normalizes its public input before selecting a PNG color type.

| Input pixel format | Palette requirement | Effective writer input | PNG representation |
| --- | --- | --- | --- |
| `PAL1`, `PAL2`, `PAL4` | Required | Expanded to one `PAL8` index byte per pixel | Indexed, color type 3, bit depth 8 |
| `PAL8` | Required | Used without index conversion | Indexed, color type 3, bit depth 8 |
| `G8` without palette | None | Gray byte replicated to R, G, and B | RGB, color type 2, 8 bits per channel |
| `G8` with palette | Optional palette is used as a lookup table | Each byte is looked up as a palette index and expanded to RGB | RGB, color type 2, 8 bits per channel |
| `RGB888` | None | Used without channel conversion | RGB, color type 2, 8 bits per channel |
| `RGB565`, `RGB555`, `BGR565`, `BGR555`, `GA88`, `AG88`, `BGR888` | None | Normalized to `RGB888` | RGB, color type 2, 8 bits per channel |
| `RGBFLOAT32`, `LINEARRGBFLOAT32`, `OKLABFLOAT32`, `CIELABFLOAT32`, `DIN99DFLOAT32` | None | Normalized to `RGB888` | RGB, color type 2, 8 bits per channel |
| `RGBA8888` | None | Used without channel conversion | RGBA, color type 6, 8 bits per channel |
| `ARGB8888`, `BGRA8888`, `ABGR8888` | None | Reordered to `RGBA8888` | RGBA, color type 6, 8 bits per channel |
| `XRGB8888`, `RGBX8888`, `XBGR8888`, `BGRX8888` | None | Reordered to `RGBA8888` with alpha forced to 255 | RGBA, color type 6, 8 bits per channel |

Unsupported pixel-format values return `SIXEL_BAD_ARGUMENT`. Palette formats without a palette also return `SIXEL_BAD_ARGUMENT`.

## Indexed PNG contract

Indexed output always uses PNG color type 3 at bit depth 8, even when the public input was packed `PAL1`, `PAL2`, or `PAL4`. Each output sample is the normalized palette index; the writer does not remap indices to make the palette smaller.

The `PLTE` length is derived from the largest referenced index: the writer emits entries zero through the maximum index, with a minimum of one and a maximum of 256 entries. Each entry is copied as the supplied 8-bit RGB triple. There is no separate `ncolors` argument on `sixel_helper_write_image_file()`, so unreferenced palette entries above the maximum index are intentionally omitted.

The writer does not emit `tRNS`, gamma, color-space, ICC, text, time, or other ancillary chunks. Indexed transparency is therefore not part of this output contract. Omitted SIXEL pixels have already been resolved by the decoder before the writer receives the index plane.

## Direct 8-bit PNG contract

Direct output is either PNG color type 2 (`RGB888`) or color type 6 (`RGBA8888`), always at 8 bits per channel. `sixel2png -dk_undither` is a representative RGB path because dequantization produces `RGB888`; `sixel2png -D` is the direct RGBA path. If `-D` is combined with a dequantizer, the dequantized RGB result is promoted back to RGBA using the decoder's direct alpha plane.

Float32 and packed integer inputs are converted before the backend is selected, so the PNG writer never emits 16-bit integer or floating-point samples. The writer emits no color-profile or transfer-function metadata; any required color-space conversion must have happened before this boundary.

## Backend selection

Backend selection is compile-time and is not a command-line choice. Autotools uses libpng when `./configure --with-png` succeeds and uses the builtin backend with `--without-png`; the default is automatic detection. Meson uses the equivalent `-Dpng=enabled`, `-Dpng=disabled`, or automatic feature selection. `HAVE_LIBPNG=1` selects the libpng branch in `src/writer.c`; otherwise the builtin branch is compiled.

Both backends implement the same pixel-format and PNG color-type contracts above. Compressed bytes, row filters, IDAT partitioning, and total file size are backend implementation details and are not byte-stability guarantees across libpng and builtin builds.

### libpng backend

The libpng backend builds one row pointer per image row and configures a non-interlaced PNG with base compression and filter types. Indexed output sets an 8-bit palette IHDR and copies the derived palette range into `PLTE`. Direct output sets an 8-bit RGB or RGBA IHDR.

libsixel supplies libpng with write and flush callbacks backed by the selected `FILE *`. A short write or flush error enters libpng's error path. Where `setjmp` and `longjmp` support is available, libsixel installs error and warning callbacks, preserves libpng's diagnostic as the additional error message, and returns `SIXEL_PNG_ERROR` after a libpng failure. Timeline logging records the write-call and byte counts without changing PNG contents.

### builtin backend

The builtin backend has separate indexed and direct implementations. It does not require libpng.

For indexed output, libsixel prefixes every row with PNG filter type 0, compresses the filtered index stream with the bundled stb zlib compressor, and writes exactly one `IHDR`, one `PLTE`, one `IDAT`, and one `IEND` after the PNG signature. Every chunk length and integer field is big-endian. Chunk CRCs use the PNG reflected CRC-32 convention: type bytes followed by payload bytes, low-bit-first shifting, reversed polynomial `0xedb88320`, initial and final complement.

For direct RGB and RGBA output, libsixel passes the normalized tightly packed rows to the bundled `stbi_write_png_to_mem()` implementation and writes the returned PNG buffer to the destination stream. Filtering and compression choices in this branch belong to the bundled implementation and are not a stable byte-level API.

## Test coverage

<!-- test-coverage: enforced -->

Each row is one independently reportable observation. Format tests run against whichever backend a build selected; the CI build matrix is responsible for exercising libpng-enabled and libpng-disabled configurations.

| ID | Contract | Backend | Owning test |
| --- | --- | --- | --- |
| PW-01 | `sixel2png` without an explicit output writes a PNG stream to standard output. | Both | [tests/cli/sixel2png/0005_stdin_default_output.t](../../tests/cli/sixel2png/0005_stdin_default_output.t) |
| PW-02 | `img2sixel -o png:-` writes the post-SIXEL PNG snapshot to standard output. | Both | [tests/codec/format/0001_prefixed_png_output_stdout.t](../../tests/codec/format/0001_prefixed_png_output_stdout.t) |
| PW-03 | `img2sixel -o png:<path>` writes the PNG snapshot to the requested path. | Both | [tests/codec/format/0002_prefixed_png_output_explicit_path.t](../../tests/codec/format/0002_prefixed_png_output_explicit_path.t) |
| PW-04 | A `.png` output suffix selects PNG snapshot output. | Both | [tests/codec/format/0003_filename_driven_png_header.t](../../tests/codec/format/0003_filename_driven_png_header.t) |
| PW-05 | The default palette decoder path writes PNG bit depth 8 and color type 3. | Both | [tests/writer/png/0001_indexed8_ihdr.t](../../tests/writer/png/0001_indexed8_ihdr.t) |
| PW-06 | The representative dequantized path writes PNG bit depth 8 and color type 2. | Both | [tests/writer/png/0002_rgb8_ihdr.t](../../tests/writer/png/0002_rgb8_ihdr.t) |
| PW-07 | Direct decoder output writes PNG bit depth 8 and color type 6. | Both | [tests/writer/png/0003_rgba8_ihdr.t](../../tests/writer/png/0003_rgba8_ihdr.t) |
| PW-08 | The builtin indexed writer emits the standards-compliant reflected CRC-32 value for the fixed IHDR vector. | Builtin | [tests/writer/png/0004_builtin_ihdr_crc.t](../../tests/writer/png/0004_builtin_ihdr_crc.t) |
| PW-09 | With Floyd-Steinberg dithering selected, the `img2sixel` PNG snapshot is byte-identical to decoding a separately generated SIXEL stream with the same writer backend. | Both | [tests/writer/png/0005_img2sixel_post_sixel_snapshot.t](../../tests/writer/png/0005_img2sixel_post_sixel_snapshot.t) |

### Coverage audit and boundary

Before the dedicated writer tests, existing coverage established that PNG targets produced files or streams and that the direct decoder option completed, but it did not assert indexed versus direct IHDR types, exercise a standard CRC vector, or prove that the quality snapshot came after SIXEL generation. The former duplicate `png:<path>` format tests also observed the same path behavior; PW-02 now owns stdout selection while PW-03 owns explicit-path selection.

The focused suite now covers the three writer output classes used by the decoder, both command entry routes, all documented `img2sixel` PNG target forms, the builtin CRC regression, and the post-SIXEL dither snapshot contract. The same format tests are backend-neutral and run in both libpng and builtin CI configurations; PW-08 is intentionally builtin-only.

The suite does not currently enumerate every public input normalization format, assert the derived `PLTE` extent, inject allocator or stream failures, validate every dynamically sized chunk CRC independently, or define high-color and animation snapshots as indexed-exact output. Those are explicit coverage gaps rather than implied guarantees. Compressed PNG byte identity across the two backends is deliberately outside the contract.
