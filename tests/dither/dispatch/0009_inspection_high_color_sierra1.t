#!/bin/sh
# Ensure inspection mode accepts high color conversion with Sierra-1.
#
# Steps:
# - Read a standard RGB test image.
# - Run img2sixel with inspection (-I) and -d sierra:variant=1.
# - Only confirm that the command exits successfully.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
target_txt="${ARTIFACT_LOCAL_DIR}/inspection.txt"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -I -d sierra:variant=1 "${input_image}" >"${target_txt}" || {
    echo "not ok" 1 - "inspection with high color and Sierra-1 failed"
    exit 0
}

echo "ok" 1 - "inspection with high color and Sierra-1 exits cleanly"

exit 0
