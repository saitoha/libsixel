#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify environment base selection.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    exit 0
}

echo "1..1"
set -v

# Captures below are compared; create their per-test directory lazily.
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! -= balanced:B1 -Z progressive -g -S -l force \
    "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" \
    >"${ARTIFACT_LOCAL_DIR}/0001_environment_base-reference.six" || {
    echo "not ok 1 - reference conversion failed"
    exit 0
}
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! -= balanced:B1 --env SIXEL_UPDATE_POLICY=progressive \
    "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" \
    >"${ARTIFACT_LOCAL_DIR}/0001_environment_base-actual.six" || {
    echo "not ok 1 - selected conversion failed"
    exit 0
}
cmp -s "${ARTIFACT_LOCAL_DIR}/0001_environment_base-reference.six" \
    "${ARTIFACT_LOCAL_DIR}/0001_environment_base-actual.six" || {
    echo "not ok 1 - environment base selection"
    exit 0
}
echo "ok 1 - environment base selection"
exit 0
