#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify explicit static survives policy reset.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    exit 0
}

echo "1..1"
set -v

# Captures below are compared; create their per-test directory lazily.
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! -= greedy:B1 -Z full -S \
    "${TOP_SRCDIR}/tests/data/inputs/formats/gif-transparent-anim-dispose3.gif" \
    >"${ARTIFACT_LOCAL_DIR}/0004_explicit_static_preserved-reference.six" || {
    echo "not ok 1 - reference conversion failed"
    exit 0
}
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! -= greedy:B1 -Z progressive -S -Z full \
    "${TOP_SRCDIR}/tests/data/inputs/formats/gif-transparent-anim-dispose3.gif" \
    >"${ARTIFACT_LOCAL_DIR}/0004_explicit_static_preserved-actual.six" || {
    echo "not ok 1 - selected conversion failed"
    exit 0
}
cmp -s "${ARTIFACT_LOCAL_DIR}/0004_explicit_static_preserved-reference.six" \
    "${ARTIFACT_LOCAL_DIR}/0004_explicit_static_preserved-actual.six" || {
    echo "not ok 1 - explicit static survives policy reset"
    exit 0
}
echo "ok 1 - explicit static survives policy reset"
exit 0
