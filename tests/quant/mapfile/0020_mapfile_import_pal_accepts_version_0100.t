#!/bin/sh
# TAP test: JASC-PAL import accepts version line 0100.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
pal_palette="${TOP_SRCDIR}/tests/data/inputs/mapfile/pal-version-0100-valid.pal"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -m "${pal_palette}" \
    "${snake_png}" -o/dev/null || {
    echo "not ok" 1 - "PAL version 0100 should be accepted"
    exit 0
}

echo "ok" 1 - "PAL version 0100 is accepted"

exit 0
