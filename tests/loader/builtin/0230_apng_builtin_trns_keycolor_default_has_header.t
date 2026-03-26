#!/bin/sh
# Verify builtin APNG default output contains the keycolor DCS header.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgba_loop2.png"
out_default="${ARTIFACT_LOCAL_DIR}/builtin-apng-trns-keycolor-default-header.six"
keycolor_header="$(printf '\033P0;1q')"

run_img2sixel -Lbuiltin! \
              -d fs -y raster \
              "${input_png}" >"${out_default}" || {
    echo "not ok 1 - builtin APNG default render failed"
    exit 0
}

if LC_ALL=C grep -a -q "${keycolor_header}" "${out_default}"; then
    echo "ok 1 - builtin APNG default output keeps keycolor DCS header"
else
    echo "not ok 1 - builtin APNG default output lost keycolor DCS header"
fi

exit 0
