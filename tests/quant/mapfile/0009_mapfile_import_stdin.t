#!/bin/sh
# TAP test: GPL palette import from stdin converts image data.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
gpl_palette="${ARTIFACT_LOCAL_DIR}/palette-stdin.gpl"

run_img2sixel -M gpl:"${gpl_palette}" -o "${ARTIFACT_LOCAL_DIR}/pal-gpl.six"         "${snake_png}" || {
    echo "not ok" 1 "Preparing GPL palette for stdin import failed"
    exit 0
}

run_img2sixel -m gpl:-         -o "${ARTIFACT_LOCAL_DIR}/from-stdin.six" "${snake_png}" <"${gpl_palette}" || {
    echo "not ok" 1 "GPL stdin palette conversion failed"
    exit 0
}

test -s "${ARTIFACT_LOCAL_DIR}/from-stdin.six" || {
    echo "not ok" 1 "GPL stdin palette conversion produced no data"
    exit 0
}

echo "ok" 1 "GPL palette import from stdin works"

exit 0
