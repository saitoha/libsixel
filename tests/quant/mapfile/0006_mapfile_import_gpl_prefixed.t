#!/bin/sh
# TAP test: GPL palette input via type prefix converts image data.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
gpl_palette="${ARTIFACT_LOCAL_DIR}/palette-gpl.dat"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -M gpl:"${gpl_palette}" -o "${ARTIFACT_LOCAL_DIR}/pal-gpl.six"         "${snake_png}" || {
    echo "not ok" 1 - "Preparing GPL palette for import failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -m gpl:"${gpl_palette}"         -o "${ARTIFACT_LOCAL_DIR}/from-gpl.six" "${snake_png}" || {
    echo "not ok" 1 - "GPL palette conversion failed"
    exit 0
}

test -s "${ARTIFACT_LOCAL_DIR}/from-gpl.six" || {
    echo "not ok" 1 - "GPL palette conversion produced no data"
    exit 0
}

echo "ok" 1 - "GPL palette input via type prefix works"

exit 0
