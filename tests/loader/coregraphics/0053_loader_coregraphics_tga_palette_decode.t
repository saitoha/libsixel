#!/bin/sh
# TAP test: coregraphics loader keeps MS-SSIM baseline for palette TGA input.

# Assess the explicitly sized encoder output, after source decoding and
# nearest resampling but before any post-encoding enlargement. The independent
# reference uses the same integer replication; native sample tests stay small.
# Regenerate: python3 tests/data/loader/quality/generate_references.py

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_COREGRAPHICS-}" = 1 || {
    printf "1..0 # SKIP coregraphics support is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

lsqa_floor=0.98
image_path="${TOP_SRCDIR}/tests/data/inputs/formats/snake-tga-type1-pal8.tga"
reference_path="${TOP_SRCDIR}/tests/data/loader/quality/7400d220-snake-64-reference-rgb.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/coregraphics_tga_palette.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -r nearest -w 192 -h 192 -L coregraphics! "${image_path}" >"${output_sixel}" || {
    echo "not ok" 1 - "coregraphics failed to decode palette TGA input"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:${lsqa_floor}" \
    "${reference_path}" "${output_sixel}" 2>&1) || {
    echo "not ok" 1 - "$lsqa_msg"
    exit 0
}

echo "ok" 1 - "coregraphics keeps MS-SSIM baseline for palette TGA input"
exit 0
