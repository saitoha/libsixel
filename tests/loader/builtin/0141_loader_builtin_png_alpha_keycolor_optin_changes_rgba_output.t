#!/bin/sh
# Verify builtin loader alpha-keycolor opt-in changes RGBA output.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/rgba.png"
out_default="${ARTIFACT_LOCAL_DIR}/builtin_alpha_keycolor_rgba_default.six"
out_optin="${ARTIFACT_LOCAL_DIR}/builtin_alpha_keycolor_rgba_optin.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 \
              -4adaptive-grid \
              -Lbuiltin:cms_engine=none! \
              "${input_png}" >"${out_default}" || {
    echo "not ok" 1 - "builtin default grayscale16+tRNS decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=2 \
              -4adaptive-grid \
              -Lbuiltin:cms_engine=none! \
              "${input_png}" >"${out_optin}" || {
    echo "not ok" 1 - "builtin opt-in RGBA decode failed"
    exit 0
}

! cmp -s "${out_default}" "${out_optin}" || {
    echo "not ok" 1 - "builtin alpha-keycolor opt-in did not change RGBA output"
    exit 0
}

echo "ok" 1 - "builtin alpha-keycolor opt-in changes RGBA output"
exit 0
