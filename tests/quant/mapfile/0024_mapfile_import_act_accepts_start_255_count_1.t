#!/bin/sh
# TAP test: ACT import accepts trailer start=255 and count=1.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
act_palette="${TOP_SRCDIR}/tests/data/inputs/mapfile/act-start-255-count-1.act"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -m "${act_palette}" \
    "${snake_png}" -o/dev/null || {
    echo "not ok" 1 - "ACT start=255,count=1 should be accepted"
    exit 0
}

echo "ok" 1 - "ACT start=255,count=1 is accepted"

exit 0
