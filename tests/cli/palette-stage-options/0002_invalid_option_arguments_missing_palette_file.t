#!/bin/sh
# TAP test ensuring img2sixel rejects missing palette files.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -m "${ARTIFACT_LOCAL_DIR}/invalid_filename" \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg" </dev/null >/dev/null  && {
    echo "not ok" 1 - "unexpected success: missing palette file"
    exit 0
}

echo "ok" 1 - "invalid option rejected"
exit 0
