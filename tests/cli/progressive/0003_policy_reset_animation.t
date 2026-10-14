#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify policy reset restores animation loading.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    exit 0
}

echo "1..1"
set -v

# Captures below are compared; create their per-test directory lazily.
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! -= 1 -Z full -g -l disable \
    "${TOP_SRCDIR}/tests/data/inputs/formats/gif-transparent-anim-dispose3.gif" \
    >"${ARTIFACT_LOCAL_DIR}/0003_policy_reset_animation-reference.six" || {
    echo "not ok 1 - reference conversion failed"
    exit 0
}
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! -= 1 -Z progressive -Z full -g -l disable \
    "${TOP_SRCDIR}/tests/data/inputs/formats/gif-transparent-anim-dispose3.gif" \
    >"${ARTIFACT_LOCAL_DIR}/0003_policy_reset_animation-actual.six" || {
    echo "not ok 1 - selected conversion failed"
    exit 0
}
cmp -s "${ARTIFACT_LOCAL_DIR}/0003_policy_reset_animation-reference.six" \
    "${ARTIFACT_LOCAL_DIR}/0003_policy_reset_animation-actual.six" || {
    echo "not ok 1 - policy reset restores animation loading"
    exit 0
}
echo "ok 1 - policy reset restores animation loading"
exit 0
