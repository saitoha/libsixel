#!/bin/sh
# TAP test: ACT palette import by extension converts image data.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
act_palette="${ARTIFACT_LOCAL_DIR}/palette.act"

run_img2sixel -M "${act_palette}" -o "${ARTIFACT_LOCAL_DIR}/act.six"         "${snake_png}" || {
    echo "not ok" 1 - "Preparing ACT palette for import failed"
    exit 0
}

run_img2sixel -m "${act_palette}" -o "${ARTIFACT_LOCAL_DIR}/from-act.six"         "${snake_png}" || {
    echo "not ok" 1 - "ACT palette conversion failed"
    exit 0
}

test -s "${ARTIFACT_LOCAL_DIR}/from-act.six" || {
    echo "not ok" 1 - "ACT palette conversion produced no data"
    exit 0
}

echo "ok" 1 - "ACT palette import by extension works"

exit 0
