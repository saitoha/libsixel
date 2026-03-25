#!/bin/sh
# Verify linear background interpretation changes YUVA composition for libwebp.

set -eux

test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP libwebp support is disabled in this build\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

command -v cwebp >/dev/null 2>&1 || {
    printf "1..0 # SKIP cwebp is unavailable\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/images/pngsuite/background/bgan6a08.png"
input_webp="${ARTIFACT_LOCAL_DIR}/webp-bgcs-lossy-alpha.webp"
output_gamma="${ARTIFACT_LOCAL_DIR}/webp-bgcs-yuva-gamma.six"
output_linear="${ARTIFACT_LOCAL_DIR}/webp-bgcs-yuva-linear.six"

cwebp -q 75 -alpha_q 100 "${input_png}" -o "${input_webp}" >/dev/null 2>&1 || {
    echo "not ok" 1 - "cwebp lossy alpha encode failed"
    exit 0
}

run_img2sixel --env SIXEL_LOADER_BACKGROUND_COLORSPACE=gamma \
              -Llibwebp:cms_engine=none! \
              -B#808080 "${input_webp}" >"${output_gamma}" || {
    echo "not ok" 1 - "libwebp YUVA gamma background composition failed"
    exit 0
}

run_img2sixel --env SIXEL_LOADER_BACKGROUND_COLORSPACE=linear \
              -Llibwebp:cms_engine=none! \
              -B#808080 "${input_webp}" >"${output_linear}" || {
    echo "not ok" 1 - "libwebp YUVA linear background composition failed"
    exit 0
}

lsqa_msg=$(set +xv; run_lsqa -m MS-SSIM -b "MS-SSIM:0.999" "${output_gamma}" "${output_linear}" 2>&1) || lsqa_status=$?

test "${lsqa_status-0}" -eq 5 || {
    echo "not ok" 1 - "gamma and linear YUVA composition were not distinguishable: ${lsqa_msg-}"
    exit 0
}

echo "ok" 1 - "linear background interpretation changes libwebp YUVA composition"
exit 0
