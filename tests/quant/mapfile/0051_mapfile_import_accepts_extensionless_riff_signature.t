#!/bin/sh
# TAP test: extensionless RIFF palette is accepted by signature detection.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
riff_palette="${TOP_SRCDIR}/tests/data/inputs/mapfile/riff-valid-noext"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin -m "${riff_palette}" \
    "${snake_png}" -o/dev/null || {
    echo "not ok" 1 - "extensionless RIFF palette should be accepted"
    exit 0
}

echo "ok" 1 - "extensionless RIFF signature is accepted"

exit 0
