# C API compatibility with v1.8.7

Applications that pass raw pixels to libsixel can still load `libsixel.so.1` after an upgrade and see different colors or image dimensions. This reference compares v1.8.7 and v1.8.7-r2 with the current library. It covers changes that can require caller updates, including differences in accepted options, output buffer layout, and allocation limits. For buffer layouts and SIXEL drawing commands, see [Pixel formats and alpha representation](concepts/pixelformat.md) and the [SIXEL format overview](sixel-format/README.md).

## Questions

Which previously successful calls can fail after an upgrade? Which buffers, command lines, or color settings need a change? How can applications distinguish a rendering regression from a rounding improvement?

## Conditions

On Linux arm64, a C client compiled and linked against each release's own header and shared library ran first with that library, then with `develop` at `cc1d092444` and with the current library. Revision `d6fa3d3fd3` corrected packed grayscale and still exhibited the BGR888 regression. The probe verified the loaded library. Both release tags produced the same observations below. The 81 release function declarations and ELF exports, public typedefs, 13 enum layouts, 81 enum members and 20 pixel-format constants remained compatible on this target. Of 147 public object-like integer macros, only `SIXEL_ALLOCATE_BYTES_MAX` changed.

For `sixel_encoder_encode_bytes()`, the input was a 9×6 checkerboard with two alternating samples, packed separately on each row. The encoder used RGB palette registers and no dithering. The comparison resolved each of the 54 output pixels to its RGB color. SIXEL stores each register component as a whole percentage, so a decoded 8-bit value can differ slightly from the original sample.

## Results

### Raw buffers passed to `sixel_encoder_encode_bytes()`

The `G1`, `G2`, and `G4` rows give decoded brightness on a 0–255 scale. The current encoder matches the release levels across all 2, 4, and 16 codes respectively. Both tested `develop` revisions returned `SIXEL_OK` even when distinct samples encoded as black.

| Input format and alternating samples | v1.8.7 and v1.8.7-r2 | `develop` at `71dc7cc159` | Current encoder |
| --- | --- | --- | --- |
| `BGR888` bytes (208, 80, 16) and (104, 32, 208) | Success. Decoded RGB (15, 79, 209) and (209, 33, 105) | `SIXEL_LOGIC_ERROR` (`0x1200`), zero output bytes | Success. Both decoded colors match the releases. |
| `G1` codes 0 and 1 | 0 and 255 | 0 and 0 | 0 and 255 |
| `G2` codes 1 and 2 | 84 and 171 | 0 and 3 | 84 and 171 |
| `G4` codes 5 and 10 | 84 and 171 | 5 and 10 | 84 and 171 |
| `AG88` or `GA88`, gray/alpha pairs (80, 231) and (208, 255) in the specified byte order | `SIXEL_LOGIC_ERROR` (`0x1200`), zero output bytes | Success. Decoded gray 79 and 209. | Success. Decoded gray 79 and 209. |

### Short indexed images passed to `sixel_encode()`

The low-level encoder took a 3×2 `PAL8` image with index rows `2 0 1` and `2 1 0` and a fixed three-color palette. Both streams declared a 3×2 raster. Decoding both streams with the same `develop` decoder isolated the effect of the encoder's first graphics new line (`-`), which advances the cursor by six pixel rows.

| Input | v1.8.7 and v1.8.7-r2 | `develop` at `71dc7cc159` |
| --- | --- | --- |
| 3×2, partial first band | An initial `-` precedes the first painted pixel. Decoded size 3×8, with six blank rows above the image. | Painting begins on row 0. Decoded size 3×2. |
| 3×6, full first band | Decoded size 3×6, painting begins on row 0. | Same dimensions and colors at every pixel. |

### Other calls and options that affect applications

The following observations apply to both `develop` at `cc1d092444` and the library containing the grayscale fix.

| Call or option | v1.8.7 and v1.8.7-r2 | Current behavior and migration |
| --- | --- | --- |
| `sixel_helper_scale_image()`, nearest, 2×1 → 3×1 | For `G8` samples 36 and 202, wrote three bytes, all 36. For `RGB565`, wrote six bytes. | These inputs produce three RGB888 pixels (nine bytes). For `G8` their brightnesses are 36, 36 and 202. Size the destination for three bytes per output pixel for either input. The helper takes no destination length. |
| `sixel_allocator_malloc()`, `calloc()`, `realloc()` | The public limit was 1,343,225,856 bytes. A request for 134,217,729 bytes reached a custom allocator. | The limit is 134,217,728 bytes (128 MiB). Exactly that size reaches the callback. One byte more returns NULL before the callback. This also applies to an executable built with the release header. |
| `sixel_encoder_setopt()` with `-e` and `-b gray2` on the same encoder | Both calls succeeded in either order. | Each mode works alone. Setting the other mode returns `SIXEL_BAD_ARGUMENT`. Choose one palette mode, or create a fresh encoder to change modes. |
| `img2sixel -s histgram` and `SIXEL_OPTFLAG_SELECT_COLOR` with `"histgram"` | Accepted the misspelling as well as `histogram`. | The misspelling is retired. Use `-s histogram` or pass `"histogram"` to the C setter. The CLI reports an option error and the setter returns `SIXEL_BAD_ARGUMENT` for the retired spelling. |
| `sixel_dither_set_complexion_score()` and `-C` | The score weighted red-channel error. For RGB (90,80,0) with palette entries (120,80,0) and (90,130,0), changing the setter from 1 to 4 switched to the second entry. | The deprecated control is accepted and has no effect. The same fixture selects the first entry with either score. See [quantization controls](encoder/palette-construction/quantization/README.md#deprecated-complexion-score). |

The `sixel_frame_clip()` probe crops a 7×3 rectangle from each of the 20 formats. The current library also accepts 11 byte-aligned layouts the releases rejected, including BGR888 and RGBA8888. The crop preserves channel order and palette indices. For packed gray, both versions published the unpacked low-bit codes as `G8`. Callers that take those cropped bytes directly should check their brightness range.

The decoder probe paints three registers from a literal SIXEL stream. HLS (150°, 45%, 37%) decoded to RGB (156,115,71) in both releases and (158,115,71) in the current library. The test allows one percentage point of wire rounding against the mathematical color. A pure red HLS register and an exactly representable RGB register provide independent controls. The probes compare displayed pixel colors and positions without requiring identical encoded streams.

## Interpretation

Pass BGR triplets directly with `SIXEL_PIXELFORMAT_BGR888` to the current encoder. Each of the 54 decoded pixels matched the corresponding RGB888 input and the release colors. Applications targeting an affected revision can swap red and blue before passing `SIXEL_PIXELFORMAT_RGB888` to work around the `sixel_frame_convert_to_rgb888: invalid pixelformat.` error.

Pass packed `G1`, `G2`, or `G4` directly to the current `sixel_encoder_encode_bytes()`. It expands each sample to full-range `G8` while unpacking its row. Code zero maps to black and the maximum code maps to white.

Callers working around the affected revision can unpack each row with `sixel_helper_normalize_pixelformat()`. This public helper returns the original low-bit codes in byte storage. When explicitly supplying `G8`, scale those codes once by 255, 85, or 17 for `G1`, `G2`, or `G4` respectively. The resulting image reproduced all 54 release pixels in each case.

Applications that used a gray-alpha encoding error to trigger a fallback will take the success path for the tested samples. Applications that cropped six leading rows from short indexed images should check the decoded dimensions before cropping. The low-level `sixel_encode()` fixture also checks 18 layouts with fixed palettes, including BGR888. That call produced the expected colors for BGR888 on every tested version. These fixtures exercise in-memory APIs. Format-specific file loaders and physical terminal rendering require separate tests.

## Reproduction

Run these commands from the root of a Linux checkout containing both release tags and the measured `develop` commit. They require Git, a C compiler, GNU make, binutils, and libpng development headers. See the [build requirements](platform-support.md#build-requirements).

The commands build three shared libraries in a temporary directory, then run the same probe against each release and the committed checkout. The client source supplies the exact row padding, palettes, and expected pixels. See the [compatibility probe](../tests/compat/v1.8.7-r2/check.sh) and [client source](../tests/compat/v1.8.7-r2/legacy-client.inc.c). The [public API cases](../tests/compat/v1.8.7-r2/public-operations.inc.c) include the scale, allocation, HLS and option fixtures. The allocation callbacks record sizes without allocating large buffers.

```sh
set -eu
export CC="${CC:-gcc}"
work=$(mktemp -d)
revision=HEAD
build_library() (
    ref=$1
    shift
    mkdir "$work/$ref"
    git archive "$ref" | tar -x -C "$work/$ref"
    cd "$work/$ref"
    ./configure --disable-static "$@"
    make -j4 -C src
)
build_library "$revision" --with-png --without-lcms2 --without-coregraphics
for tag in v1.8.7 v1.8.7-r2; do
    build_library "$tag" --without-png --without-jpeg CFLAGS="-O2 -std=gnu11"
    sh tests/compat/v1.8.7-r2/check.sh \
        "$work/$tag/include/sixel.h" "$work/$tag/src/.libs/libsixel.so" \
        "$work/$revision/src/.libs/libsixel.so" "$work/$revision/include/sixel.h" \
        "${tag#v}"
done
```

The probe requires successful BGR888 encoding and release-correct decoded colors for BGR888 and all three packed grayscale formats. To reproduce all four failures, set `revision=71dc7cc15960345ea8ba759f177e3aba30449ab0`. At `revision=d6fa3d3fd3` only BGR888 fails. The [Linux CI driver](../.github/scripts/check-color-compat.sh) also checks every pixel of a 5×7 four-color image supplied in both RGB and BGR order, including the partial final SIXEL band. Its 17×6 grayscale ramps cover every code and the last pixel of each padded row.
