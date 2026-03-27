#!/bin/sh
# TAP test confirming final img2sixel output keeps/transforms transparency
# headers for librsvg input as expected.

set -eux

test "${HAVE_LIBRSVG-}" = 1 || {
    printf "1..0 # SKIP librsvg loader is unavailable in this build\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n"
    exit 0
}

command -v python3 >/dev/null 2>&1 || {
    printf "1..0 # SKIP python3 is unavailable in this environment\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

svg_path="${TOP_SRCDIR}/tests/data/inputs/formats/librsvg-transparent-2color.svg"
default_sixel="${ARTIFACT_LOCAL_DIR}/librsvg-transparent-default.six"
bg_sixel="${ARTIFACT_LOCAL_DIR}/librsvg-transparent-bg.six"
default_png="${ARTIFACT_LOCAL_DIR}/librsvg-transparent-default.png"
bg_png="${ARTIFACT_LOCAL_DIR}/librsvg-transparent-bg.png"
header_alpha="${ARTIFACT_LOCAL_DIR}/librsvg-header-alpha.bin"
header_opaque="${ARTIFACT_LOCAL_DIR}/librsvg-header-opaque.bin"

printf '\033P0;1q' >"${header_alpha}"
printf '\033Pq' >"${header_opaque}"

run_img2sixel -L librsvg! "${svg_path}" >"${default_sixel}" || {
    echo "not ok" 1 - "default transparent SVG conversion failed"
    exit 0
}

run_img2sixel -L librsvg! -B '#ffffff' "${svg_path}" >"${bg_sixel}" || {
    echo "not ok" 1 - "background-composited SVG conversion failed"
    exit 0
}

dd if="${default_sixel}" bs=1 count=6 2>/dev/null | cmp -s - "${header_alpha}" || {
    echo "not ok" 1 - "transparent SVG did not emit ESC P0;1q header"
    exit 0
}

dd if="${bg_sixel}" bs=1 count=3 2>/dev/null | cmp -s - "${header_opaque}" || {
    echo "not ok" 1 - "background SVG did not emit ESC Pq header"
    exit 0
}

dd if="${bg_sixel}" bs=1 count=6 2>/dev/null | cmp -s - "${header_alpha}" && {
    echo "not ok" 1 - "background SVG unexpectedly kept ESC P0;1q header"
    exit 0
}

run_sixel2png -i "${default_sixel}" -o "${default_png}" || {
    echo "not ok" 1 - "default transparent SIXEL decode failed"
    exit 0
}

run_sixel2png -i "${bg_sixel}" -o "${bg_png}" || {
    echo "not ok" 1 - "background-composited SIXEL decode failed"
    exit 0
}

python3 - "${default_png}" "${bg_png}" <<'PYEOF' || {
import struct
import sys
import zlib


def paeth_predictor(left, up, up_left):
    predictor = left + up - up_left
    left_distance = abs(predictor - left)
    up_distance = abs(predictor - up)
    up_left_distance = abs(predictor - up_left)
    if left_distance <= up_distance and left_distance <= up_left_distance:
        return left
    if up_distance <= up_left_distance:
        return up
    return up_left


def decode_png_rgba(path):
    data = open(path, "rb").read()
    if not data.startswith(b"\x89PNG\r\n\x1a\n"):
        raise RuntimeError("invalid PNG signature")

    cursor = 8
    width = 0
    height = 0
    color_type = 0
    bit_depth = 0
    plte = None
    trns = None
    idat = []

    while cursor < len(data):
        chunk_len = struct.unpack(">I", data[cursor:cursor + 4])[0]
        cursor += 4
        chunk_type = data[cursor:cursor + 4]
        cursor += 4
        chunk_data = data[cursor:cursor + chunk_len]
        cursor += chunk_len
        cursor += 4  # CRC

        if chunk_type == b"IHDR":
            (width, height, bit_depth, color_type,
             compression, filtering, interlace) = struct.unpack(
                ">IIBBBBB", chunk_data
            )
            if bit_depth != 8 or compression != 0 or filtering != 0 or interlace != 0:
                raise RuntimeError("unsupported PNG encoding")
        elif chunk_type == b"PLTE":
            plte = chunk_data
        elif chunk_type == b"tRNS":
            trns = chunk_data
        elif chunk_type == b"IDAT":
            idat.append(chunk_data)
        elif chunk_type == b"IEND":
            break

    if width <= 0 or height <= 0:
        raise RuntimeError("missing IHDR")

    bytes_per_pixel_by_type = {2: 3, 3: 1, 6: 4}
    if color_type not in bytes_per_pixel_by_type:
        raise RuntimeError("unsupported PNG color type")

    bytes_per_pixel = bytes_per_pixel_by_type[color_type]
    row_bytes = width * bytes_per_pixel
    raw = zlib.decompress(b"".join(idat))
    rgba = []
    cursor = 0
    previous = bytearray(row_bytes)

    for _ in range(height):
        filter_type = raw[cursor]
        cursor += 1
        row = bytearray(raw[cursor:cursor + row_bytes])
        cursor += row_bytes

        if filter_type == 1:
            for index in range(row_bytes):
                row[index] = (row[index] + (row[index - bytes_per_pixel]
                                            if index >= bytes_per_pixel else 0)) & 0xFF
        elif filter_type == 2:
            for index in range(row_bytes):
                row[index] = (row[index] + previous[index]) & 0xFF
        elif filter_type == 3:
            for index in range(row_bytes):
                left = row[index - bytes_per_pixel] if index >= bytes_per_pixel else 0
                row[index] = (row[index] + ((left + previous[index]) // 2)) & 0xFF
        elif filter_type == 4:
            for index in range(row_bytes):
                left = row[index - bytes_per_pixel] if index >= bytes_per_pixel else 0
                up = previous[index]
                up_left = previous[index - bytes_per_pixel] if index >= bytes_per_pixel else 0
                row[index] = (row[index] + paeth_predictor(left, up, up_left)) & 0xFF
        elif filter_type != 0:
            raise RuntimeError("unsupported PNG filter type")

        previous = row
        if color_type == 6:
            for index in range(0, row_bytes, 4):
                rgba.append((row[index], row[index + 1], row[index + 2], row[index + 3]))
        elif color_type == 2:
            for index in range(0, row_bytes, 3):
                rgba.append((row[index], row[index + 1], row[index + 2], 255))
        else:  # color_type == 3
            if plte is None:
                raise RuntimeError("palette image without PLTE")
            for palette_index in row:
                palette_offset = palette_index * 3
                red = plte[palette_offset]
                green = plte[palette_offset + 1]
                blue = plte[palette_offset + 2]
                alpha = trns[palette_index] if trns is not None and palette_index < len(trns) else 255
                rgba.append((red, green, blue, alpha))

    return width, height, rgba


def is_redish(pixel):
    red, green, blue, _alpha = pixel
    return red >= 180 and green <= 80 and blue <= 80


def is_light(pixel, threshold):
    red, green, blue, _alpha = pixel
    return red >= threshold and green >= threshold and blue >= threshold


default_path = sys.argv[1]
bg_path = sys.argv[2]
default_width, default_height, default_pixels = decode_png_rgba(default_path)
bg_width, bg_height, bg_pixels = decode_png_rgba(bg_path)

if (default_width, default_height) != (2, 1):
    raise RuntimeError("unexpected default decode geometry")
if (bg_width, bg_height) != (2, 1):
    raise RuntimeError("unexpected bgcolor decode geometry")

if not is_redish(default_pixels[0]):
    raise RuntimeError("default decode left pixel is not red-ish")
if not is_redish(bg_pixels[0]):
    raise RuntimeError("bgcolor decode left pixel is not red-ish")

if not is_light(default_pixels[1], 200):
    raise RuntimeError("default decode right pixel is not light enough")
if not is_light(bg_pixels[1], 240):
    raise RuntimeError("bgcolor decode right pixel is not white-ish")
PYEOF
    echo "not ok" 1 - "restored PNG pixels did not match transparency/composition expectations"
    exit 0
}

echo "ok" 1 - "librsvg transparency header routing is correct end-to-end"
exit 0
