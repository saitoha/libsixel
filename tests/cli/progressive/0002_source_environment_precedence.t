#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify explicit source index precedence.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    exit 0
}

echo "1..1"
set -v

# Captures below are compared; create their per-test directory lazily.
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! -= greedy:B1 -Z progressive -T 2 \
    "${TOP_SRCDIR}/tests/data/inputs/formats/gif-transparent-anim-dispose3.gif" \
    >"${ARTIFACT_LOCAL_DIR}/0002_source_environment_precedence-reference.six" || {
    echo "not ok 1 - reference conversion failed"
    exit 0
}
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! -= greedy:B1 --env SIXEL_LOADER_ANIMATION_START_FRAME_NO=1 -Z progressive -T 2 \
    "${TOP_SRCDIR}/tests/data/inputs/formats/gif-transparent-anim-dispose3.gif" \
    >"${ARTIFACT_LOCAL_DIR}/0002_source_environment_precedence-actual.six" || {
    echo "not ok 1 - selected conversion failed"
    exit 0
}
cmp -s "${ARTIFACT_LOCAL_DIR}/0002_source_environment_precedence-reference.six" \
    "${ARTIFACT_LOCAL_DIR}/0002_source_environment_precedence-actual.six" || {
    echo "not ok 1 - explicit source index precedence"
    exit 0
}
echo "ok 1 - explicit source index precedence"
exit 0
