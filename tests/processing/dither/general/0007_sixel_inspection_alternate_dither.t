#!/bin/sh
# Inspect Sixel with alternate ordered dither configuration.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

status=0


snake_six="${TOP_SRCDIR}/images/map8.six"
target_txt="${ARTIFACT_LOCAL_DIR}/sixel-inspection-alt-dither.txt"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -I -da_dither -w100 "${snake_six}" >"${target_txt}" || {
    echo "not ok" 1 - "alternate ordered dither inspection fails"
    exit "${status}"
}

echo "ok" 1 - "alternate ordered dither inspection works"
exit "${status}"
