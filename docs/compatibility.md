# C API compatibility with v1.8.7

Applications that pass raw pixels to libsixel can still load `libsixel.so.1` after an upgrade and see different colors or image dimensions. This reference records the encoder behavior measured on v1.8.7, v1.8.7-r2, and `develop` at `71dc7cc159`. For buffer layouts and SIXEL drawing commands, see [Pixel formats and alpha representation](concepts/pixelformat.md) and the [SIXEL format overview](sixel-format/README.md).

## Questions

Which raw pixel formats change an encoder call's status or decoded colors? Does a short indexed image start on the same row? Which input conversions reproduce the v1.8.7 result?

## Conditions

On Linux arm64, a C client compiled and linked against each release's own header and shared library ran first with that library, then with the `develop` library. The probe verified which library was loaded. Both release tags produced the same results below. The 20 public pixel-format constants retained their numeric values, and all 81 public functions exported by the release library were present in the tested `develop` library.

For `sixel_encoder_encode_bytes()`, the input was a 9×6 checkerboard with two alternating samples, packed separately on each row. The encoder used RGB palette registers and no dithering. The comparison resolved each of the 54 output pixels to its RGB color. SIXEL stores each register component as a whole percentage, so a decoded 8-bit value can differ slightly from the original sample.

## Results

### Raw buffers passed to `sixel_encoder_encode_bytes()`

The `G1`, `G2`, and `G4` rows give decoded brightness on a 0–255 scale. The encoder returns `SIXEL_OK` in all three builds for these formats, so checking the return status alone misses the loss of contrast.

| Input format and alternating samples | v1.8.7 and v1.8.7-r2 | `develop` at `71dc7cc159` |
| --- | --- | --- |
| `BGR888` bytes (208, 80, 16) and (104, 32, 208) | Success. Decoded RGB (15, 79, 209) and (209, 33, 105) | `SIXEL_LOGIC_ERROR` (`0x1200`), zero output bytes. The diagnostic is `sixel_frame_convert_to_rgb888: invalid pixelformat.` |
| `G1` codes 0 and 1 | 0 and 255 | 0 and 0. Both samples appear black. |
| `G2` codes 1 and 2 | 84 and 171 | 0 and 3 |
| `G4` codes 5 and 10 | 84 and 171 | 5 and 10 |
| `AG88` or `GA88`, gray/alpha pairs (80, 231) and (208, 255) in the specified byte order | `SIXEL_LOGIC_ERROR` (`0x1200`), zero output bytes | Success. Decoded gray 79 and 209. |

### Short indexed images passed to `sixel_encode()`

The low-level encoder took a 3×2 `PAL8` image with index rows `2 0 1` and `2 1 0` and a fixed three-color palette. Both streams declared a 3×2 raster. Decoding both streams with the same `develop` decoder isolated the effect of the encoder's first graphics new line (`-`), which advances the cursor by six pixel rows.

| Input | v1.8.7 and v1.8.7-r2 | `develop` at `71dc7cc159` |
| --- | --- | --- |
| 3×2, partial first band | An initial `-` precedes the first painted pixel. Decoded size 3×8, with six blank rows above the image. | Painting begins on row 0. Decoded size 3×2. |
| 3×6, full first band | Decoded size 3×6, painting begins on row 0. | Same dimensions and colors at every pixel. |

## Interpretation

For `BGR888`, swap the red and blue bytes in each pixel and pass the resulting buffer as `SIXEL_PIXELFORMAT_RGB888`. The release image and the converted input produced the same 54 decoded pixels.

For packed grayscale, unpack each row separately. The public `sixel_helper_normalize_pixelformat()` helper can do this, but its `G8` result still contains the original low-bit codes. Expand each code to `code * 255 / ((1 << bits) - 1)` before passing the buffer as `SIXEL_PIXELFORMAT_G8`. For 1, 2, and 4 bits the division is exact. The expanded input reproduced all 54 release pixels in each case.

Applications that used a gray-alpha encoding error to trigger a fallback will take the success path for the tested samples. Applications that cropped six leading rows from short indexed images should check the decoded dimensions before cropping. These observations concern the two raw-buffer API calls above. File loaders and terminal rendering have their own behavior.

## Reproduction

Run these commands from the root of a Linux checkout containing both release tags and the measured `develop` commit. They require Git, a C compiler, GNU make, binutils, and libpng development headers. See the [build requirements](platform-support.md#build-requirements).

The commands build three shared libraries in a temporary directory, then run the same probe against each release and the pinned `develop` revision. The client source supplies the exact row padding, palettes, and expected pixels. See the [compatibility probe](../tests/compat/v1.8.7-r2/check.sh) and [client source](../tests/compat/v1.8.7-r2/legacy-client.inc.c).

```sh
set -eu
export CC="${CC:-gcc}"
work=$(mktemp -d)
revision=71dc7cc15960345ea8ba759f177e3aba30449ab0
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

At the pinned revision, each release comparison reports four `KNOWN REGRESSION` lines for `bgr888`, `g1`, `g2`, and `g4`. The probe accepts either the release-correct result or the specific recorded regression for each of those formats. An unexpected error or decoded pixel fails the check. To compare a later checkout against both releases, use the [Linux CI driver](../.github/scripts/check-color-compat.sh).
