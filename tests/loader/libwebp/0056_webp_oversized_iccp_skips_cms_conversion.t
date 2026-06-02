#!/bin/sh
# Verify oversized ICCP skips WebP CMS conversion without failing decode.

set -eux

test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP libwebp support is disabled in this build\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test -n "${SIXEL_TEST_GZIP-}" || {
    printf "1..0 # SKIP gzip is unavailable for compressed WebP fixture\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_webp="${TOP_BUILDDIR}/tests/data/inputs/formats/webp-static-icc-overlimit-padded.webp"
out_auto="${ARTIFACT_LOCAL_DIR}/webp-oversized-icc-cms-auto.six"
out_none="${ARTIFACT_LOCAL_DIR}/webp-oversized-icc-cms-none.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibwebp:cms_engine=auto! "${input_webp}" >"${out_auto}" || {
    echo "not ok" 1 - "libwebp oversized ICC decode failed (cms=auto)"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibwebp:cms_engine=none! "${input_webp}" >"${out_none}" || {
    echo "not ok" 1 - "libwebp oversized ICC decode failed (cms=none)"
    exit 0
}

cmp -s "${out_auto}" "${out_none}" || {
    echo "not ok" 1 - "oversized ICC did not skip CMS conversion as expected"
    exit 0
}

echo "ok" 1 - "oversized ICC skips CMS conversion while decode succeeds"
exit 0
