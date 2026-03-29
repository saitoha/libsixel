#!/bin/sh
# Verify ANIM background is applied when -B is not specified.

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
output_default="${ARTIFACT_LOCAL_DIR}/webp-anim-bg-default.six"
output_expected="${ARTIFACT_LOCAL_DIR}/webp-anim-bg-expected-112233.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibwebp:cms_engine=none! -S "${input_webp}" >"${output_default}" || {
    echo "not ok" 1 - "libwebp animation decode without -B failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibwebp:cms_engine=none! -S -B#112233 "${input_webp}" >"${output_expected}" || {
    echo "not ok" 1 - "libwebp animation decode with -B#112233 failed"
    exit 0
}

cmp -s "${output_default}" "${output_expected}" || {
    echo "not ok" 1 - "ANIM background was not applied as default output"
    exit 0
}

echo "ok" 1 - "ANIM opaque background is applied when -B is absent"
exit 0
