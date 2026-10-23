#!/bin/sh
# Verify parallel decoder parse and gather spans have both timeline boundaries.
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
timeline_log="${ARTIFACT_LOCAL_DIR}/0011-parallel-timeline-$$.jsonl"
parse_start=0
parse_finish=0
gather_start=0
gather_finish=0

${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" --threading-policy=2 \
    -J "${timeline_log}" \
    -i "${TOP_SRCDIR}/tests/data/inputs/snake_64.six" \
    -o /dev/null || {
    echo "not ok" 1 - "parallel decoder timeline conversion failed"
    exit 0
}

while IFS= read -r line; do
    case "${line}" in *\"worker\":\"decoder\",\"role\":\"parse\",\"event\":\"start\"**) false ;; *) true ;; esac || parse_start=$((parse_start + 1))
    case "${line}" in *\"worker\":\"decoder\",\"role\":\"parse\",\"event\":\"finish\"**) false ;; *) true ;; esac || parse_finish=$((parse_finish + 1))
    case "${line}" in *\"worker\":\"decoder\",\"role\":\"gather\",\"event\":\"start\"**) false ;; *) true ;; esac || gather_start=$((gather_start + 1))
    case "${line}" in *\"worker\":\"decoder\",\"role\":\"gather\",\"event\":\"finish\"**) false ;; *) true ;; esac || gather_finish=$((gather_finish + 1))
done < "${timeline_log}"

test "${parse_start}" -gt 0 || {
    echo "not ok" 1 - "parallel decoder timeline omitted parse starts"
    exit 0
}
test "${parse_start}" -eq "${parse_finish}" || {
    echo "not ok" 1 - "parallel decoder parse spans are unpaired"
    exit 0
}
test "${gather_start}" -gt 0 || {
    echo "not ok" 1 - "parallel decoder timeline omitted gather starts"
    exit 0
}
test "${gather_start}" -eq "${gather_finish}" || {
    echo "not ok" 1 - "parallel decoder gather spans are unpaired"
    exit 0
}

echo "ok" 1 - "parallel decoder timeline spans are paired"
exit 0
