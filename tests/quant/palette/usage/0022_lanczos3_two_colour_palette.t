#!/bin/sh
# Scale with Lanczos3 filter using a two-colour palette.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_jpg="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
target_sixel="${ARTIFACT_LOCAL_DIR}/lanczos3-two-colour.sixel"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -p 2 -h100 -wauto -rlanczos3 "${snake_jpg}" \
        >"${target_sixel}" || {
    echo "not ok" 1 - "Lanczos3 scaling with two-colour palette fails"
    exit 0
}

echo "ok" 1 - "Lanczos3 scaling with two-colour palette works"

exit 0
