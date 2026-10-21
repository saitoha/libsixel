#!/bin/sh
# Validate downscale 80pct scaling with nearest resampling.
# Policy: docs/encoder/preprocessing/resampling/compact-kernels.md
#
# Reference image details:
# - Source: tests/data/inputs/snake_240.png (240x240)
# - Independent reference: floor(output_index * source_size / target_size)
# - Resize target: 80% of the original dimensions
#
# The test compares img2sixel output against the prebuilt reference image.
# Regenerate with tests/data/inputs/generate_scaling_quality_fixtures.py.
# Both aligned dimensions remain >=176 after scaling.
# Calculate only the metric asserted by this quality gate.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.98}

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_240.png"
reference_image="${TOP_SRCDIR}/tests/data/inputs/scaling/snake_240_nearest_80pct.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/nearest-downscale_80pct.six"


${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -r nearest -w 80% -o "${output_sixel}" "${input_image}" || {
    echo "not ok" 1 - "nearest downscale 80pct scaling failed"
    exit 0
}

lsqa_err=$(
    set +xv
    ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:${lsqa_floor}" "${reference_image}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

test "${lsqa_run_status:-0}" -eq 0 && {
    echo "ok" 1 - "nearest downscale 80pct lsqa passed"
    exit 0
}

test "${lsqa_run_status}" -eq 5 && {
    echo "not ok" 1 - "${lsqa_err}"
    exit 0
}

echo "not ok" 1 - "nearest downscale 80pct lsqa failed"

exit 0
