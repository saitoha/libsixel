#!/bin/sh
# Ensure stacked palette files are handled correctly.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

map8_six="${TOP_SRCDIR}/images/map8.six"
snake_six="${TOP_SRCDIR}/tests/data/inputs/snake_64.six"
target_sixel="${ARTIFACT_LOCAL_DIR}/stacked-palettes.sixel"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -m "${map8_six}" -m "${map8_six}" "${snake_six}" >"${target_sixel}" || {
    echo "not ok" 1 - "stacked palette files fail"
    exit 0
}

echo "ok" 1 - "stacked palette files handled"

exit 0
