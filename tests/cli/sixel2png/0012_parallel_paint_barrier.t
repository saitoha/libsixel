#!/bin/sh
# Verify every direct paint worker reaches the barrier before paint starts.
# Policy: docs/functionality/decoding-pipeline.md

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    echo "1..0 # SKIP sixel2png is disabled in this build"
    exit 0
}
test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP thread backend is unavailable"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
timeline_log="${ARTIFACT_LOCAL_DIR}/0012-parallel-paint-barrier-$$.jsonl"
paint_ready=0
paint_start=0
paint_finish=0
paint_started_early=0

${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" --threads=2 \
    -J "${timeline_log}" \
    -i "${TOP_SRCDIR}/tests/data/inputs/snake_64.six" \
    -o /dev/null || {
    echo "not ok" 1 - "parallel decoder timeline conversion failed"
    exit 0
}

while IFS= read -r line; do
    test "${line#*\"worker\":\"decoder\",\"role\":\"paint\",\"event\":\"ready\"*}" = \
        "${line}" || paint_ready=$((paint_ready + 1))
    test "${line#*\"worker\":\"decoder\",\"role\":\"paint\",\"event\":\"start\"*}" = \
        "${line}" || {
        paint_start=$((paint_start + 1))
        test "${paint_ready}" -eq 2 || paint_started_early=1
    }
    test "${line#*\"worker\":\"decoder\",\"role\":\"paint\",\"event\":\"finish\"*}" = \
        "${line}" || paint_finish=$((paint_finish + 1))
done < "${timeline_log}"

test "${paint_ready}" -eq 2 || {
    echo "not ok" 1 - "not every paint worker reached the barrier"
    exit 0
}
test "${paint_started_early}" -eq 0 || {
    echo "not ok" 1 - "paint started before every worker was ready"
    exit 0
}
test "${paint_start}" -eq 2 || {
    echo "not ok" 1 - "not every paint worker started"
    exit 0
}
test "${paint_finish}" -eq 2 || {
    echo "not ok" 1 - "not every paint worker finished"
    exit 0
}

echo "ok" 1 - "parallel paint workers cross one release barrier"
exit 0
