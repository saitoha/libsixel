#!/bin/sh
# Policy: docs/quality/video-evaluation.md
set -eux
echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
cat >"${ARTIFACT_LOCAL_DIR}/clock.jsonl" <<'JSON'
{"update":0,"byte_end":3761,"pts":0}
JSON

status=0
${SIXEL_RUNTIME-} "${LSQA_PATH}" -Ma -Ac -C "${ARTIFACT_LOCAL_DIR}/clock.jsonl" -m S-CIELAB -L 'builtin!' --cms-engine=none "${TOP_SRCDIR}/tests/data/assessment/video-snake-16.png" "${TOP_SRCDIR}/tests/data/assessment/video-snake-16.six" >/dev/null || status=$?
test "$status" -eq 4 || {
    echo "not ok 1 - clock split packet"
    exit 0
}
echo "ok 1 - clock split packet"
exit 0
