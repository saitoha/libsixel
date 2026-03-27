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

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_webp="${TOP_SRCDIR}/tests/data/inputs/formats/webp-static-icc-overlimit-padded.webp"
out_auto="${ARTIFACT_LOCAL_DIR}/webp-oversized-icc-cms-auto.six"
out_none="${ARTIFACT_LOCAL_DIR}/webp-oversized-icc-cms-none.six"

run_img2sixel -Llibwebp:cms_engine=auto! "${input_webp}" >"${out_auto}" || {
    echo "not ok" 1 - "libwebp oversized ICC decode failed (cms=auto)"
    exit 0
}

run_img2sixel -Llibwebp:cms_engine=none! "${input_webp}" >"${out_none}" || {
    echo "not ok" 1 - "libwebp oversized ICC decode failed (cms=none)"
    exit 0
}

cmp -s "${out_auto}" "${out_none}" || {
    echo "not ok" 1 - "oversized ICC did not skip CMS conversion as expected"
    exit 0
}

echo "ok" 1 - "oversized ICC skips CMS conversion while decode succeeds"
exit 0
