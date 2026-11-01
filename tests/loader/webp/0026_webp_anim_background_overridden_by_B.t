#!/bin/sh
# Verify explicit -B overrides ANIM background for libwebp.

set -eux

test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP libwebp support is disabled in this build\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_webp="${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossless-alpha-8x8-2frame-bg112233.webp"
output_default="${ARTIFACT_LOCAL_DIR}/webp-anim-bg-default-override.six"
output_white="${ARTIFACT_LOCAL_DIR}/webp-anim-bg-white-override.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibwebp:cms_engine=none! -S "${input_webp}" >"${output_default}" || {
    echo "not ok" 1 - "libwebp animation decode without -B failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibwebp:cms_engine=none! -S -B#fff "${input_webp}" >"${output_white}" || {
    echo "not ok" 1 - "libwebp animation decode with -B#fff failed"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:0.999" "${output_default}" "${output_white}" 2>&1) || lsqa_status=$?

test "${lsqa_status-0}" -eq 5 || {
    echo "not ok" 1 - "explicit -B did not produce a distinguishable result: ${lsqa_msg-}"
    exit 0
}

echo "ok" 1 - "explicit -B overrides ANIM background"
exit 0
