#!/bin/sh
# Confirm small RGBA roundtrip retains the MS-SSIM baseline.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
printf '1..1\n'
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

# Baseline against alpha-composited reference (default black background).
lsqa_floor=0.99

image_path="${TOP_SRCDIR}/tests/data/inputs/formats/rgba.png"
expected_ppm="${TOP_SRCDIR}/tests/data/loader/builtin_expected/0005_rgba_png_default_black_composite.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/rgba_roundtrip.six"
output_png="${ARTIFACT_LOCAL_DIR}/rgba_roundtrip.png"

run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 -Lbuiltin! "${image_path}" >"${output_sixel}" || {
    echo "not ok" 1 - "rgba roundtrip encode failed"
    exit 0
}

run_sixel2png -i "${output_sixel}" -o "${output_png}" || {
    echo "not ok" 1 - "rgba roundtrip decode failed"
    exit 0
}

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${expected_ppm}" "${output_png}" 2>&1
) || lsqa_run_status=$?

lsqa_status=${lsqa_run_status-0}

test "${lsqa_status}" -ne 5 || {
    echo "not ok" 1 - "${lsqa_err}"
    exit 0
}

test "${lsqa_status}" -eq 0 || {
    echo "not ok" 1 - "rgba roundtrip ms-ssim regressed"
    exit 0
}

echo "ok" 1 - "rgba roundtrip ms-ssim meets baseline"


exit 0
