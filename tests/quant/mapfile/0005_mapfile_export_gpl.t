#!/bin/sh
# TAP test: GPL palette export writes GIMP header.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
gpl_palette="${ARTIFACT_LOCAL_DIR}/palette.gpl"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -M gpl:"${gpl_palette}" -o "${ARTIFACT_LOCAL_DIR}/pal-gpl.six" "${snake_png}" || {
    echo "not ok" 1 - "GPL palette export failed"
    exit 0
}

head -n 1 "${gpl_palette}" | grep -q "GIMP Palette" || {
    echo "not ok" 1 - "GPL palette missing GIMP header"
    exit 0
}

echo "ok" 1 - "GPL palette exported with GIMP header"

exit 0
