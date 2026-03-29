#!/bin/sh
# TAP test: ACT palette import by extension converts image data.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
act_palette="${ARTIFACT_LOCAL_DIR}/palette.act"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -M "${act_palette}" -o "${ARTIFACT_LOCAL_DIR}/act.six"         "${snake_png}" || {
    echo "not ok" 1 - "Preparing ACT palette for import failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -m "${act_palette}" -o "${ARTIFACT_LOCAL_DIR}/from-act.six"         "${snake_png}" || {
    echo "not ok" 1 - "ACT palette conversion failed"
    exit 0
}

test -s "${ARTIFACT_LOCAL_DIR}/from-act.six" || {
    echo "not ok" 1 - "ACT palette conversion produced no data"
    exit 0
}

echo "ok" 1 - "ACT palette import by extension works"

exit 0
