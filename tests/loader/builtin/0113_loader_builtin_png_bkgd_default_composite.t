#!/bin/sh
# TAP test: builtin PNG path should honor embedded bKGD when compositing alpha.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/images/pngsuite/background/bgan6a08.png"
expected_ppm="${TOP_SRCDIR}/tests/data/loader/pngsuite_expected/0062_pngsuite_background_default_bgan6a08_msssim.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/builtin_bgan6a08_default.six"

run_img2sixel -Lbuiltin:enable_cms=0! "${input_png}" >"${output_sixel}" || {
    echo "not ok" 1 - "builtin bKGD default composite conversion failed"
    exit 0
}

lsqa_msg=$(set +xv; run_lsqa -m MS-SSIM -b "MS-SSIM:0.98" "${expected_ppm}" "${output_sixel}" 2>&1) || {
    echo "not ok" 1 - "$lsqa_msg"
    exit 0
}

echo "ok" 1 - "builtin bKGD default compositing matches reference"
exit 0
