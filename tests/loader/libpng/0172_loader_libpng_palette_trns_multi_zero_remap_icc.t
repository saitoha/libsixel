#!/bin/sh
# Verify multi-zero palette+tRNS fixture matches normalized single-zero output.

set -eux

test "${HAVE_LIBPNG-}" = 1 || {
    echo "1..0 # SKIP libpng support is disabled in this build"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_multi="${TOP_SRCDIR}/tests/data/inputs/formats/libpng-pal8-trns-multi0-semi-icc.png"
input_single="${TOP_SRCDIR}/tests/data/inputs/formats/libpng-pal8-trns-single0-semi-icc.png"
out_multi="${ARTIFACT_LOCAL_DIR}/multi0-white-cms0.six"
out_single="${ARTIFACT_LOCAL_DIR}/single0-white-cms0.six"

run_img2sixel -Llibpng:cms_engine=none! \
              -B#ffffff \
              -d none -p256 \
              "${input_multi}" >"${out_multi}" || {
    echo "not ok 1 - multi-zero render failed"
    exit 0
}

run_img2sixel -Llibpng:cms_engine=none! \
              -B#ffffff \
              -d none -p256 \
              "${input_single}" >"${out_single}" || {
    echo "not ok 1 - single-zero render failed"
    exit 0
}

if cmp -s "${out_multi}" "${out_single}"; then
    echo "ok 1 - multi-zero remap matches normalized single-zero output"
else
    echo "not ok 1 - multi-zero remap differs from normalized single-zero output"
fi

exit 0
