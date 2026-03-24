#!/bin/sh
# Verify opt-in tRNS keycolor mode changes ColorType 2 output.

set -eux

test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng support is disabled in this build\n"
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

input_png="${TOP_SRCDIR}/images/pngsuite/transparency/tbrn2c08.png"
default_out="${ARTIFACT_LOCAL_DIR}/libpng_trns_keycolor_default.six"
optin_out="${ARTIFACT_LOCAL_DIR}/libpng_trns_keycolor_optin.six"

run_img2sixel --env SIXEL_LOADER_LIBPNG_TRNS_KEYCOLOR=0 \
              -Llibpng:cms=0! \
              "${input_png}" >"${default_out}" || {
    echo "not ok" 1 - "libpng default decode failed"
    exit 0
}

run_img2sixel --env SIXEL_LOADER_LIBPNG_TRNS_KEYCOLOR=1 \
              -Llibpng:cms=0! \
              "${input_png}" >"${optin_out}" || {
    echo "not ok" 1 - "libpng opt-in keycolor decode failed"
    exit 0
}

if cmp -s "${default_out}" "${optin_out}"; then
    echo "not ok" 1 - "opt-in keycolor mode did not change output"
    exit 0
fi

echo "ok" 1 - "opt-in keycolor mode changes ColorType 2+tRNS output"
exit 0
