#!/bin/sh
# TAP test: PNGSuite case for background/bgwn6a08.png with direct LSQA comparison.

# Reference image generation command (independent from img2sixel):
#   python3 tests/data/loader/pngsuite_expected/generate_png_rgba8_white_ref.py \
#       images/pngsuite/background/bgwn6a08.png \
#       tests/data/loader/pngsuite_expected/0074_pngsuite_background_white_bgwn6a08_msssim.ppm
set -eux

test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng support is disabled in this build\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/images/pngsuite/background/bgwn6a08.png"
expected_ppm="${TOP_SRCDIR}/tests/data/loader/pngsuite_expected/0074_pngsuite_background_white_bgwn6a08_msssim.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/bgwn6a08.sixel"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -B#fff -Llibpng:cms_engine=none! "${input_png}" >"${output_sixel}" || {
    echo "not ok" 1 - "img2sixel failed"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:0.977" "${expected_ppm}" "${output_sixel}" 2>&1) || {
    echo "not ok" 1 - "$lsqa_msg"
    exit 0
}

echo "ok" 1 - "background_white background/bgwn6a08.png"
exit 0
