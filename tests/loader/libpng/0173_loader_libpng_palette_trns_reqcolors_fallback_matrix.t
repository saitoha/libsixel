#!/bin/sh
# Verify low reqcolors leaves pal8 work format for indexed+tRNS fixture.

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

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/libpng-pal8-trns-multi0-semi-icc.png"
out_lo="${ARTIFACT_LOCAL_DIR}/req-lo-cms0.six"
log_lo="${ARTIFACT_LOCAL_DIR}/req-lo-cms0.log"

run_img2sixel -v -Llibpng:cms_engine=none! \
              -B#ffffff -d none -p16 \
              "${input_png}" >"${out_lo}" 2>"${log_lo}" || {
    echo "not ok 1 - low reqcolors render failed"
    exit 0
}

if grep -q 'formats: source=pal8 work=pal8' "${log_lo}"; then
    echo "not ok 1 - low reqcolors unexpectedly kept pal8 work format"
else
    echo "ok 1 - low reqcolors moved off pal8 work format"
fi

exit 0
