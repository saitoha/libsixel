# C API compatibility with v1.8.7

Applications that pass raw pixels to libsixel can still load `libsixel.so.1` after an upgrade and see different colors or image dimensions. This reference compares v1.8.7 and v1.8.7-r2 with the current encoder and records a packed grayscale regression in `develop` at `71dc7cc159`. For buffer layouts and SIXEL drawing commands, see [Pixel formats and alpha representation](concepts/pixelformat.md) and the [SIXEL format overview](sixel-format/README.md).

## Questions

Which raw pixel formats change an encoder call's status or decoded colors? Does a short indexed image start on the same row? Which input conversions reproduce the v1.8.7 result?

## Conditions

On Linux arm64, a C client compiled and linked against each release's own header and shared library ran first with that library, then with the `develop` library. The probe verified which library was loaded. Both release tags produced the same results below. The 20 public pixel-format constants retained their numeric values, and all 81 public functions exported by the release library were present in the tested `develop` library.

For `sixel_encoder_encode_bytes()`, the input was a 9×6 checkerboard with two alternating samples, packed separately on each row. The encoder used RGB palette registers and no dithering. The comparison resolved each of the 54 output pixels to its RGB color. SIXEL stores each register component as a whole percentage, so a decoded 8-bit value can differ slightly from the original sample.

## Results

### Raw buffers passed to `sixel_encoder_encode_bytes()`

The `G1`, `G2`, and `G4` rows give decoded brightness on a 0–255 scale. The current encoder matches the release levels across all 2, 4, and 16 codes respectively. The affected `develop` revision also returned `SIXEL_OK` even when distinct samples encoded as black.

| Input format and alternating samples | v1.8.7 and v1.8.7-r2 | `develop` at `71dc7cc159` | Current encoder |
| --- | --- | --- | --- |
| `BGR888` bytes (208, 80, 16) and (104, 32, 208) | Success. Decoded RGB (15, 79, 209) and (209, 33, 105) | `SIXEL_LOGIC_ERROR` (`0x1200`), zero output bytes | Same error and zero output bytes |
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

## Interpretation

For `BGR888`, the recorded diagnostic is `sixel_frame_convert_to_rgb888: invalid pixelformat.` Swap the red and blue bytes in each pixel and pass the resulting buffer as `SIXEL_PIXELFORMAT_RGB888`. The release image and the converted input produced the same 54 decoded pixels.

Pass packed `G1`, `G2`, or `G4` directly to the current `sixel_encoder_encode_bytes()`. It expands each sample to full-range `G8` while unpacking its row. Code zero maps to black and the maximum code maps to white.

Callers working around the affected revision can unpack each row with `sixel_helper_normalize_pixelformat()`. This public helper returns the original low-bit codes in byte storage. When explicitly supplying `G8`, scale those codes once by 255, 85, or 17 for `G1`, `G2`, or `G4` respectively. The resulting image reproduced all 54 release pixels in each case.

Applications that used a gray-alpha encoding error to trigger a fallback will take the success path for the tested samples. Applications that cropped six leading rows from short indexed images should check the decoded dimensions before cropping. These observations concern the two raw-buffer API calls above. File loaders and terminal rendering have their own behavior.

## Reproduction

Run these commands from the root of a Linux checkout containing both release tags and the measured `develop` commit. They require Git, a C compiler, GNU make, binutils, and libpng development headers. See the [build requirements](platform-support.md#build-requirements).

The commands build three shared libraries in a temporary directory, then run the same probe against each release and the committed checkout. The client source supplies the exact row padding, palettes, and expected pixels. See the [compatibility probe](../tests/compat/v1.8.7-r2/check.sh) and [client source](../tests/compat/v1.8.7-r2/legacy-client.inc.c).

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

The probe requires release-correct grayscale pixels. To reproduce the failures, set `revision=71dc7cc15960345ea8ba759f177e3aba30449ab0`. That comparison fails for all three packed grayscale formats. Only the recorded `BGR888` error is accepted as a known regression. The [Linux CI driver](../.github/scripts/check-color-compat.sh) also runs 17×6 ascending and descending grayscale ramps covering every code, including the last pixel of each padded row.
