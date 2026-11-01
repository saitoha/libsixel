#!/bin/sh
# Scale with Lanczos4 filter and emit palette dump.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_jpg="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
target_sixel="${ARTIFACT_LOCAL_DIR}/lanczos4-palette-dump.sixel"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -e -h140 -rlanczos4 -P "${snake_jpg}" \
        >"${target_sixel}" || {
    echo "not ok" 1 - "Lanczos4 scaling palette dump fails"
    exit 0
}

echo "ok" 1 - "Lanczos4 scaling emits palette dump"

exit 0
