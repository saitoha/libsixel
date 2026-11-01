#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# Direct selective_blur target reconstruction matches the established opaque result through stdin.
set -eux
echo "1..1"
set -v

# The reference is measured from a file so GNV pipe EOF cannot block it.
# The direct reconstruction below still exercises SIXEL input through stdin.
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -dselective_blur:threshold=24 -i "${TOP_SRCDIR}/tests/data/inputs/snake_64.six" -o "${ARTIFACT_LOCAL_DIR}/reference.png" || {
    echo "not ok 1 - reconstruction reference decode failed"
    exit 0
}
expected=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m DELTA_E00 -L builtin "${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp" "${ARTIFACT_LOCAL_DIR}/reference.png") || {
    echo "not ok 1 - reconstruction reference failed"
    exit 0
}
actual=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m DELTA_E00 -D -L builtin -dselective_blur:threshold=24 "${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp" - <"${TOP_SRCDIR}/tests/data/inputs/snake_64.six") || {
    echo "not ok 1 - direct reconstruction failed"
    exit 0
}
test "$actual" = "$expected" || {
    echo "not ok 1 - reconstruction mismatch: $actual != $expected"
    exit 0
}
echo "ok 1 - reconstruct selective blur"
exit 0
