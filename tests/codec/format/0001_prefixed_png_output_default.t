#!/bin/sh
# Confirm prefixed PNG output is created via png: scheme.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_jpg="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
prefixed_png="${ARTIFACT_LOCAL_DIR}/snake-prefixed.png"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -o "png:${prefixed_png}" "${snake_jpg}" || {
    echo "not ok" 1 - "prefixed PNG conversion failed"
    exit 0
}

test -s "${prefixed_png}" || {
    echo "not ok" 1 - "prefixed PNG output missing"
    exit 0
}

echo "ok" 1 - "prefixed PNG output created"
exit 0
