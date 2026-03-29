#!/bin/sh
# Verify process SIXEL_BGCOLOR disables builtin PNG keycolor path.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/images/pngsuite/transparency/tbwn0g16.png"
out_on="${ARTIFACT_LOCAL_DIR}/builtin_trns_keycolor_gray16_bg_on.six"
out_off="${ARTIFACT_LOCAL_DIR}/builtin_trns_keycolor_gray16_bg_off.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
              --env SIXEL_BGCOLOR=white \
              -Lbuiltin:cms_engine=none! \
              "${input_png}" >"${out_on}" || {
    echo "not ok" 1 - "process SIXEL_BGCOLOR + builtin opt-in grayscale16+tRNS decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 \
              --env SIXEL_BGCOLOR=white \
              -Lbuiltin:cms_engine=none! \
              "${input_png}" >"${out_off}" || {
    echo "not ok" 1 - "process SIXEL_BGCOLOR + builtin opt-out grayscale16+tRNS decode failed"
    exit 0
}

cmp -s "${out_on}" "${out_off}" || {
    echo "not ok" 1 - "process SIXEL_BGCOLOR unexpectedly keeps builtin PNG keycolor path"
    exit 0
}

echo "ok" 1 - "process SIXEL_BGCOLOR disables builtin PNG keycolor path"

exit 0
