#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Ensure grayscale JPEG quality stays within the recorded lsqa baseline.
# Fixture: tests/data/inputs/snake_192.png is the 192x192 source.
# Reference/fixture generation commands (ImageMagick):
#   magick tests/data/inputs/snake_192.png -colorspace Gray -depth 8 -strip tests/data/inputs/formats/snake-192-grayscale.jpg
#   magick tests/data/inputs/snake_192.png -colorspace Gray -depth 8 PPM:tests/data/inputs/formats/snake-192-reference-gray.ppm

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

printf '1..1
'
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.98}


image_path="${TOP_SRCDIR}/tests/data/inputs/formats/snake-192-grayscale.jpg"
reference_path="${TOP_SRCDIR}/tests/data/inputs/formats/snake-192-reference-gray.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/grayscale.six"
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin! "${image_path}" -o "${output_sixel}" || {
    echo "not ok" 1 - "grayscale quality regressed"
    exit 0
}

lsqa_err=$(
    set +xv
    ${SIXEL_RUNTIME-} "${LSQA_PATH}" -b "MS-SSIM:${lsqa_floor}" "${reference_path}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

lsqa_status=${lsqa_run_status-0}

test "${lsqa_status}" -ne 5 || {
    echo "not ok" 1 - "${lsqa_err}"
    exit 0
}

test "${lsqa_status}" -eq 0 || {
    echo "not ok" 1 - "grayscale quality regressed"
    exit 0
}

echo "ok" 1 - "grayscale quality meets baseline"
exit 0
