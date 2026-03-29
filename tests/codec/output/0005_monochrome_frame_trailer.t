#!/bin/sh
# Emit monochrome frame output.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_jpg="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
target_sixel="${ARTIFACT_LOCAL_DIR}/monochrome-frame.sixel"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -p 1 -h100 -n1 "${snake_jpg}" >"${target_sixel}" || {
    echo "not ok" 1 - "monochrome frame conversion fails"
    exit 0
}

echo "ok" 1 - "monochrome frame conversion succeeds"
exit 0
