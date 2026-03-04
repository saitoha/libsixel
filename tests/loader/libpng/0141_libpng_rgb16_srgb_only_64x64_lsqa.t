#!/bin/sh
# TAP test: libpng should decode 16-bit sRGB PNG (without iCCP/gAMA/cHRM)
# and keep visual parity against the 64x64 PNM reference.

set -eux

test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng support is disabled in this build\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
echo "1..1"
set -v

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/snake_64_rgb16_srgb_only.png"
expected_ppm="${TOP_SRCDIR}/tests/data/loader/libpng_expected/0141_libpng_rgb16_srgb_only_64x64_expected.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/snake_64_rgb16_srgb_only.sixel"

test -f "${input_png}" || {
    echo "not ok" 1 "missing test fixture"
    exit 0
}
test -f "${expected_ppm}" || {
    echo "not ok" 1 "missing reference PPM"
    exit 0
}

# Validate IHDR: width=64, height=64, bit depth=16.
width=$(dd if="${input_png}" bs=1 skip=16 count=4 2>/dev/null \
    | od -An -tu1 \
    | awk 'NF {print ($1 * 16777216) + ($2 * 65536) + ($3 * 256) + $4; exit}')
height=$(dd if="${input_png}" bs=1 skip=20 count=4 2>/dev/null \
    | od -An -tu1 \
    | awk 'NF {print ($1 * 16777216) + ($2 * 65536) + ($3 * 256) + $4; exit}')
bit_depth=$(dd if="${input_png}" bs=1 skip=24 count=1 2>/dev/null \
    | od -An -tu1 \
    | tr -d '[:space:]')
if [ "${width}" != "64" ] || [ "${height}" != "64" ] || [ "${bit_depth}" != "16" ]; then
    echo "not ok" 1 "fixture IHDR mismatch (${width}x${height}, depth=${bit_depth})"
    exit 0
fi

# Validate color-management chunk composition of fixture.
grep -a -q 'sRGB' "${input_png}" || {
    echo "not ok" 1 "fixture missing sRGB chunk"
    exit 0
}
for banned_chunk in iCCP gAMA cHRM; do
    if grep -a -q "${banned_chunk}" "${input_png}"; then
        echo "not ok" 1 "fixture has unexpected ${banned_chunk} chunk"
        exit 0
    fi
done

run_img2sixel -Llibpng:enable_cms=0! "${input_png}" >"${output_sixel}" || {
    echo "not ok" 1 "img2sixel failed"
    exit 0
}

lsqa_msg=$(set +xv; run_lsqa -m MS-SSIM -b "MS-SSIM:0.99" "${expected_ppm}" "${output_sixel}" 2>&1) || {
    echo "not ok" 1 "$lsqa_msg"
    exit 0
}

echo "ok" 1 "libpng rgb16 sRGB-only fixture matches 64x64 reference"
exit 0
