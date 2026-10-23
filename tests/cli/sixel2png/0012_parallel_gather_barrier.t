#!/bin/sh
# Verify every direct gather worker reaches the barrier before gather starts.
# Policy: docs/decoder/pipeline/README.md

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
timeline_log="${ARTIFACT_LOCAL_DIR}/0012-parallel-gather-barrier-$$.jsonl"
gather_ready=0
gather_start=0
gather_finish=0
gather_started_early=0

${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" --threading-policy=2 \
    -J "${timeline_log}" \
    -i "${TOP_SRCDIR}/tests/data/inputs/snake_64.six" \
    -o /dev/null || {
    echo "not ok" 1 - "parallel decoder timeline conversion failed"
    exit 0
}

while IFS= read -r line; do
    case "${line}" in *\"worker\":\"decoder\",\"role\":\"gather\",\"event\":\"ready\"**) false ;; *) true ;; esac || gather_ready=$((gather_ready + 1))
    case "${line}" in *\"worker\":\"decoder\",\"role\":\"gather\",\"event\":\"start\"**) false ;; *) true ;; esac || {
        gather_start=$((gather_start + 1))
        test "${gather_ready}" -eq 2 || gather_started_early=1
    }
    case "${line}" in *\"worker\":\"decoder\",\"role\":\"gather\",\"event\":\"finish\"**) false ;; *) true ;; esac || gather_finish=$((gather_finish + 1))
done < "${timeline_log}"

test "${gather_ready}" -eq 2 || {
    echo "not ok" 1 - "not every gather worker reached the barrier"
    exit 0
}
test "${gather_started_early}" -eq 0 || {
    echo "not ok" 1 - "gather started before every worker was ready"
    exit 0
}
test "${gather_start}" -eq 2 || {
    echo "not ok" 1 - "not every gather worker started"
    exit 0
}
test "${gather_finish}" -eq 2 || {
    echo "not ok" 1 - "not every gather worker finished"
    exit 0
}

echo "ok" 1 - "parallel gather workers cross one release barrier"
exit 0
