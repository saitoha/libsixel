#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo '1..0 # SKIP img2sixel is disabled in this build'
    exit 0
}

echo '1..1'
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
input="${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -p 16 -Qheckbert -d none \
    --threading-policy=greedy:global_budget=1 -4auto:solid=on \
    "${input}" >"${ARTIFACT_LOCAL_DIR}/reference.six" || {
    echo 'not ok 1 - reference conversion failed'
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -p 16 -Qheckbert -d none \
    --threading-policy=greedy:global_budget=1 --sampling-policy=adaptive-grid:solid=off \
    "${input}" >"${ARTIFACT_LOCAL_DIR}/actual.six" || {
    echo 'not ok 1 - actual conversion failed'
    exit 0
}

comparison=0
cmp -s "${ARTIFACT_LOCAL_DIR}/reference.six"     "${ARTIFACT_LOCAL_DIR}/actual.six" || comparison=$?
test "${comparison}" -eq 1 || {
    echo 'not ok 1 - solid=off changes output with solid regions'
    exit 0
}
echo 'ok 1 - solid=off changes output with solid regions'
exit 0
