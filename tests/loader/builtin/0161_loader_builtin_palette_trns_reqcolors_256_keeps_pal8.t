#!/bin/sh
# Verify reqcolors=256 keeps pal8 work format in builtin path.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/libpng-pal8-trns-multi0-semi-icc.png"
out_hi="${ARTIFACT_LOCAL_DIR}/builtin-req256-cms0.six"
log_hi="${ARTIFACT_LOCAL_DIR}/builtin-req256-cms0.log"

run_img2sixel -v -Lbuiltin:cms_engine=none! \
              -B#ffffff -d none -p256 \
              "${input_png}" >"${out_hi}" 2>"${log_hi}" || {
    echo "not ok 1 - builtin reqcolors=256 render failed"
    exit 0
}

if grep -q 'formats: source=pal8 work=pal8' "${log_hi}"; then
    echo "ok 1 - builtin reqcolors=256 keeps pal8 work format"
else
    echo "not ok 1 - builtin reqcolors=256 did not keep pal8 work format"
fi

exit 0
