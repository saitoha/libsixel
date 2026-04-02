#!/bin/sh
# TAP test: ACT import keeps count=0 compatibility by treating it as 256.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
act_palette="${TOP_SRCDIR}/tests/data/inputs/mapfile/act-count-zero.act"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin -m "${act_palette}" \
    "${snake_png}" -o/dev/null || {
    echo "not ok" 1 - "ACT trailer count zero should be accepted"
    exit 0
}

echo "ok" 1 - "ACT trailer count zero keeps 256-color compatibility"

exit 0
