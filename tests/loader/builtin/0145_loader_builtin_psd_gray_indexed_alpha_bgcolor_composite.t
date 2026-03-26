#!/bin/sh
# Verify builtin PSD Gray/Indexed alpha compositing responds to --bgcolor.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..2"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

test_no=1
for case_entry in \
    "stbi_minimal_gray8_alpha.psd:grayscale+alpha" \
    "stbi_minimal_indexed8_alpha.psd:indexed+alpha"
do
    file_name=${case_entry%%:*}
    case_desc=${case_entry#*:}
    input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/${file_name}"
    output_black_six="${ARTIFACT_LOCAL_DIR}/${file_name%.psd}_bg_black.six"
    output_white_six="${ARTIFACT_LOCAL_DIR}/${file_name%.psd}_bg_white.six"
    output_black_png="${ARTIFACT_LOCAL_DIR}/${file_name%.psd}_bg_black.png"
    output_white_png="${ARTIFACT_LOCAL_DIR}/${file_name%.psd}_bg_white.png"

    run_img2sixel -Lbuiltin! -B "#000000" "${input_psd}" >"${output_black_six}" || {
        echo "not ok" "${test_no}" - "builtin PSD decode failed with black bgcolor (${case_desc})"
        exit 0
    }

    run_img2sixel -Lbuiltin! -B "#ffffff" "${input_psd}" >"${output_white_six}" || {
        echo "not ok" "${test_no}" - "builtin PSD decode failed with white bgcolor (${case_desc})"
        exit 0
    }

    run_sixel2png -i "${output_black_six}" -o "${output_black_png}" || {
        echo "not ok" "${test_no}" - "black bgcolor sixel2png decode failed (${case_desc})"
        exit 0
    }

    run_sixel2png -i "${output_white_six}" -o "${output_white_png}" || {
        echo "not ok" "${test_no}" - "white bgcolor sixel2png decode failed (${case_desc})"
        exit 0
    }

    lsqa_status=0
    lsqa_msg=$(set +xv; run_lsqa -m MS-SSIM -b "MS-SSIM:0.99" \
        "${output_black_png}" "${output_white_png}" 2>&1) || lsqa_status=$?

    test "${lsqa_status}" -eq 5 || {
        echo "not ok" "${test_no}" - "PSD bgcolor did not affect output (${case_desc}): ${lsqa_msg}"
        exit 0
    }

    echo "ok" "${test_no}" - "builtin PSD ${case_desc} compositing changes with bgcolor"
    test_no=$((test_no + 1))
done

exit 0
