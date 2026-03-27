#!/bin/sh
# Verify ANIM background channel order maps to RGB correctly.

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
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_webp="${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossless-alpha-8x8-2frame-bg112233.webp"
out_default="${ARTIFACT_LOCAL_DIR}/webp-anim-bg-default-order.six"
out_expected="${ARTIFACT_LOCAL_DIR}/webp-anim-bg-expected-112233.six"
out_legacy_bug="${ARTIFACT_LOCAL_DIR}/webp-anim-bg-legacy-2211ff.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibwebp:cms_engine=none! -S "${input_webp}" >"${out_default}" || {
    echo "not ok" 1 - "libwebp animation decode without -B failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibwebp:cms_engine=none! -S -B#112233 "${input_webp}" >"${out_expected}" || {
    echo "not ok" 1 - "libwebp animation decode with expected -B failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibwebp:cms_engine=none! -S -B#2211ff "${input_webp}" >"${out_legacy_bug}" || {
    echo "not ok" 1 - "libwebp animation decode with legacy-bug -B failed"
    exit 0
}

cmp -s "${out_default}" "${out_expected}" || {
    echo "not ok" 1 - "ANIM background did not match expected RGB channel order"
    exit 0
}

cmp -s "${out_default}" "${out_legacy_bug}" && {
    echo "not ok" 1 - "ANIM background still matches legacy channel-order bug"
    exit 0
}

echo "ok" 1 - "ANIM background uses correct RGB channel order"
exit 0
