#!/bin/sh
# Verify builtin loader opt-in keycolor mode changes grayscale16+tRNS output.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/images/pngsuite/transparency/tbwn0g16.png"
out_default="${ARTIFACT_LOCAL_DIR}/builtin_trns_keycolor_gray16_default.six"
out_optin="${ARTIFACT_LOCAL_DIR}/builtin_trns_keycolor_gray16_optin.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 \
              -Lbuiltin:cms_engine=none! \
              "${input_png}" >"${out_default}" || {
    echo "not ok" 1 - "builtin default grayscale16+tRNS decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
              -Lbuiltin:cms_engine=none! \
              "${input_png}" >"${out_optin}" || {
    echo "not ok" 1 - "builtin opt-in grayscale16+tRNS decode failed"
    exit 0
}

cmp -s "${out_default}" "${out_optin}" && {
    echo "not ok" 1 - "builtin opt-in keycolor mode did not change grayscale16+tRNS output"
    exit 0
}

echo "ok" 1 - "builtin opt-in keycolor mode changes grayscale16+tRNS output"
exit 0
