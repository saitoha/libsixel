#!/bin/sh
# Convert with a 16-colour palette using the fast encoder.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_jpg="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
map16_palette="${TOP_SRCDIR}/images/map16-palette.png"
target_sixel="${ARTIFACT_LOCAL_DIR}/palette16.sixel"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -7 -m "${map16_palette}" -Efast "${snake_jpg}" >"${target_sixel}" || {
    echo "not ok" 1 - "16-colour palette conversion fails"
    exit 0
}

echo "ok" 1 - "16-colour palette conversion succeeds"

exit 0
