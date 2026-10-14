#!/bin/sh
# Policy: docs/quality/video-evaluation.md
set -eux
echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

printf 'sentinel\n' >"${ARTIFACT_LOCAL_DIR}/frames.jsonl"
set +e
${SIXEL_RUNTIME-} "${LSQA_PATH}" -Ma -L 'builtin!' --cms-engine=none -m S-CIELAB -o "${ARTIFACT_LOCAL_DIR}/frames.jsonl" "${TOP_SRCDIR}/tests/data/assessment/video-snake-16.png" "${TOP_SRCDIR}/tests/data/assessment/video-snake-16-truncated.six" >/dev/null
status=$?
set -e
test "${status}" -eq 4 || {
    echo "not ok 1 - late packet rejection"
    exit 0
}
IFS= read -r value <"${ARTIFACT_LOCAL_DIR}/frames.jsonl"
test "${value}" = sentinel || {
    echo "not ok 1 - report preservation"
    exit 0
}

echo "ok 1 - late error preserves report"
exit 0
