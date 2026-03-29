#!/bin/sh
# TAP test: coregraphics interlaced GIF static decode output is non-empty.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_COREGRAPHICS-}" = 1 || {
    printf "1..0 # SKIP coregraphics support is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L coregraphics! -ldisable -S \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-gif-interlaced.gif" \
    >"${ARTIFACT_LOCAL_DIR}/coregraphics_interlaced_static.six" || {
    echo "not ok" 1 - "static interlaced GIF decode failed on coregraphics"
    exit 0
}

test -s "${ARTIFACT_LOCAL_DIR}/coregraphics_interlaced_static.six" || {
    echo "not ok" 1 - "static interlaced GIF decode produced empty output"
    exit 0
}

echo "ok" 1 - "coregraphics emits non-empty static output for interlaced GIF"
exit 0
