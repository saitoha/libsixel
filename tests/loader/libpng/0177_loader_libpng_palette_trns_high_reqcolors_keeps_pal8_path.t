#!/bin/sh
# Verify high reqcolors keeps pal8 work format for indexed+tRNS fixture.

set -eux

test "${HAVE_LIBPNG-}" = 1 || {
    echo "1..0 # SKIP libpng support is disabled in this build"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}


echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/libpng-pal8-trns-multi0-semi-icc.png"
out_hi="${ARTIFACT_LOCAL_DIR}/req-hi-cms0.six"
log_hi="${ARTIFACT_LOCAL_DIR}/req-hi-cms0.log"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v -Llibpng:cms_engine=none! \
              -B#ffffff -d none -p256 \
              "${input_png}" >"${out_hi}" 2>"${log_hi}" || {
    echo "not ok 1 - high reqcolors render failed"
    exit 0
}

grep -q 'formats: source=pal8 work=pal8' "${log_hi}" || {
    echo "not ok 1 - high reqcolors did not keep pal8 work format"
    exit 0
}

    echo "ok 1 - high reqcolors keeps pal8 work format"


exit 0
