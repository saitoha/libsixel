#!/bin/sh
# Confirm prefixed PNG output respects explicit path.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_jpg="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
target_png="${ARTIFACT_LOCAL_DIR}/snake-explicit.png"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -o "png:${target_png}" "${snake_jpg}" || {
    echo "not ok" 1 - "prefixed PNG conversion failed"
    exit 0
}

test -s "${target_png}" || {
    echo "not ok" 1 - "prefixed PNG did not produce file"
    exit 0
}

echo "ok" 1 - "prefixed PNG writes to explicit path"
exit 0
