#!/bin/sh
# TAP test: RIFF palette import by explicit type converts image data.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
riff_palette="${ARTIFACT_LOCAL_DIR}/palette-riff.pal"
riff_alias="${ARTIFACT_LOCAL_DIR}/palette-riff.alias"

run_img2sixel -M pal-riff:"${riff_palette}"         -o "${ARTIFACT_LOCAL_DIR}/pal-riff.six" "${snake_png}" || {
    echo "not ok" 1 - "Preparing RIFF palette for import failed"
    exit 0
}

cat "${riff_palette}" >"${riff_alias}"

run_img2sixel -m pal-riff:"${riff_alias}"         -o "${ARTIFACT_LOCAL_DIR}/from-riff.six" "${snake_png}" || {
    echo "not ok" 1 - "RIFF palette conversion failed"
    exit 0
}

test -s "${ARTIFACT_LOCAL_DIR}/from-riff.six" || {
    echo "not ok" 1 - "RIFF palette conversion produced no data"
    exit 0
}

echo "ok" 1 - "RIFF palette import using explicit type works"

exit 0
