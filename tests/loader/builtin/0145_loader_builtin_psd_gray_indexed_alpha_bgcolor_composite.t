#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify builtin PSD grayscale+alpha compositing responds to --bgcolor.
# Fixture generation command:
#   python3 tests/data/inputs/formats/generate_psd_snake16_fixtures.py

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/snake16_gray8_alpha.psd"
output_black_six="${ARTIFACT_LOCAL_DIR}/snake16_gray8_alpha_bg_black.six"
output_white_six="${ARTIFACT_LOCAL_DIR}/snake16_gray8_alpha_bg_white.six"
output_black_png="${ARTIFACT_LOCAL_DIR}/snake16_gray8_alpha_bg_black.png"
output_white_png="${ARTIFACT_LOCAL_DIR}/snake16_gray8_alpha_bg_white.png"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin! -B "#000000" "${input_psd}" >"${output_black_six}" || {
    echo "not ok" 1 - "builtin PSD decode failed with black bgcolor (grayscale+alpha)"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin! -B "#ffffff" "${input_psd}" >"${output_white_six}" || {
    echo "not ok" 1 - "builtin PSD decode failed with white bgcolor (grayscale+alpha)"
    exit 0
}

${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -i "${output_black_six}" -o "${output_black_png}" || {
    echo "not ok" 1 - "black bgcolor sixel2png decode failed (grayscale+alpha)"
    exit 0
}

${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -i "${output_white_six}" -o "${output_white_png}" || {
    echo "not ok" 1 - "white bgcolor sixel2png decode failed (grayscale+alpha)"
    exit 0
}

# The contract is an observable change in composited pixel colors. A
# zero maximum color difference means every compared pixel stayed equal.
# Maximum color error is a direct inequality observation here, with no
# image-quality threshold and no unavailable-result-as-rejection shortcut.
color_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m DE00_max \
    "${output_black_png}" "${output_white_png}") || {
    echo "not ok 1 - composited pixel comparison failed"
    exit 0
}
case "${color_error}" in
    ""|0.000000|*[!0-9.]*)
        echo "not ok 1 - background must produce a finite nonzero color change"
        exit 0
        ;;
esac

echo "ok" 1 - "builtin PSD grayscale+alpha compositing changes with bgcolor"
exit 0
