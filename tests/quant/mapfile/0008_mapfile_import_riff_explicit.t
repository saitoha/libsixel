#!/bin/sh
# TAP test: RIFF palette import by explicit type converts image data.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
riff_alias="${ARTIFACT_LOCAL_DIR}/palette-riff.alias"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin -M pal-riff:"${riff_alias}"         -o "${ARTIFACT_LOCAL_DIR}/pal-riff.six" "${snake_png}" || {
    echo "not ok" 1 - "Preparing RIFF palette for import failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin -m pal-riff:"${riff_alias}"         -o "${ARTIFACT_LOCAL_DIR}/from-riff.six" "${snake_png}" || {
    echo "not ok" 1 - "RIFF palette conversion failed"
    exit 0
}

test -s "${ARTIFACT_LOCAL_DIR}/from-riff.six" || {
    echo "not ok" 1 - "RIFF palette conversion produced no data"
    exit 0
}

echo "ok" 1 - "RIFF palette import using explicit type works"

exit 0
