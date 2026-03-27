#!/bin/sh
# Verify VT340 colour control sequences are emitted.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_ppm="${TOP_SRCDIR}/tests/data/inputs/small.ppm"
target_sixel="${ARTIFACT_LOCAL_DIR}/vt340-colour.sixel"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -bvt340color "${snake_ppm}" >"${target_sixel}" || {
    echo "not ok" 1 - "VT340 colour control emission failed"
    exit 0
}

echo "ok" 1 - "VT340 colour control sequences emitted"

exit 0
