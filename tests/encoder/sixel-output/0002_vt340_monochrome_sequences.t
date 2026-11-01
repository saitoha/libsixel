#!/bin/sh
# Verify VT340 monochrome control sequences are emitted.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_tga="${TOP_SRCDIR}/tests/data/inputs/snake_64.tga"
target_sixel="${ARTIFACT_LOCAL_DIR}/vt340-mono.sixel"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -bvt340mono "${snake_tga}" >"${target_sixel}" || {
    echo "not ok" 1 - "VT340 monochrome control failed"
    exit 0
}

echo "ok" 1 - "VT340 monochrome control sequences emitted"

exit 0
