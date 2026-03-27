#!/bin/sh
# TAP test: coregraphics static frame output differs from default animation output.

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
mkdir -p "${ARTIFACT_LOCAL_DIR}"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L coregraphics! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" \
    >"${ARTIFACT_LOCAL_DIR}/coregraphics_animated_default.six" || {
    echo "not ok" 1 - "default animated GIF decode failed on coregraphics"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L coregraphics! -ldisable -S \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" \
    >"${ARTIFACT_LOCAL_DIR}/coregraphics_animated_static.six" || {
    echo "not ok" 1 - "static animated GIF decode failed on coregraphics"
    exit 0
}

cmp -s "${ARTIFACT_LOCAL_DIR}/coregraphics_animated_default.six" \
    "${ARTIFACT_LOCAL_DIR}/coregraphics_animated_static.six" && {
    echo "not ok" 1 - "default and static coregraphics GIF outputs unexpectedly match"
    exit 0
}

echo "ok" 1 - "coregraphics static frame output differs from default animation output"
exit 0
