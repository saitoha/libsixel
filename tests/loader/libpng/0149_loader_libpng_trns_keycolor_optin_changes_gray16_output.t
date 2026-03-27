#!/bin/sh
# Verify opt-in tRNS keycolor mode changes ColorType 0/16-bit output.

set -eux

test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng support is disabled in this build\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/images/pngsuite/transparency/tbwn0g16.png"
default_out="${ARTIFACT_LOCAL_DIR}/libpng_trns_keycolor_gray16_default.six"
optin_out="${ARTIFACT_LOCAL_DIR}/libpng_trns_keycolor_gray16_optin.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 \
              -Llibpng:cms_engine=none! \
              "${input_png}" >"${default_out}" || {
    echo "not ok" 1 - "libpng default grayscale16 decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
              -Llibpng:cms_engine=none! \
              "${input_png}" >"${optin_out}" || {
    echo "not ok" 1 - "libpng opt-in grayscale16 keycolor decode failed"
    exit 0
}

cmp -s "${default_out}" "${optin_out}" && {
    echo "not ok" 1 - "opt-in keycolor mode did not change grayscale16 output"
    exit 0
}


echo "ok" 1 - "opt-in keycolor mode changes ColorType 0/16-bit+tRNS output"
exit 0
