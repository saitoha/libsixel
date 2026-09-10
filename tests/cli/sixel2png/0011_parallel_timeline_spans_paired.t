#!/bin/sh
# Verify parallel decoder scan and paint spans have both timeline boundaries.
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
timeline_log="${ARTIFACT_LOCAL_DIR}/0011-parallel-timeline-$$.jsonl"
scan_start=0
scan_finish=0
paint_start=0
paint_finish=0

${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" --threads=2 \
    -J "${timeline_log}" \
    -i "${TOP_SRCDIR}/tests/data/inputs/snake_64.six" \
    -o /dev/null || {
    echo "not ok" 1 - "parallel decoder timeline conversion failed"
    exit 0
}

while IFS= read -r line; do
    test "${line#*\"worker\":\"decoder\",\"role\":\"scan\",\"event\":\"start\"*}" = \
        "${line}" || scan_start=$((scan_start + 1))
    test "${line#*\"worker\":\"decoder\",\"role\":\"scan\",\"event\":\"finish\"*}" = \
        "${line}" || scan_finish=$((scan_finish + 1))
    test "${line#*\"worker\":\"decoder\",\"role\":\"paint\",\"event\":\"start\"*}" = \
        "${line}" || paint_start=$((paint_start + 1))
    test "${line#*\"worker\":\"decoder\",\"role\":\"paint\",\"event\":\"finish\"*}" = \
        "${line}" || paint_finish=$((paint_finish + 1))
done < "${timeline_log}"

test "${scan_start}" -gt 0 || {
    echo "not ok" 1 - "parallel decoder timeline omitted scan starts"
    exit 0
}
test "${scan_start}" -eq "${scan_finish}" || {
    echo "not ok" 1 - "parallel decoder scan spans are unpaired"
    exit 0
}
test "${paint_start}" -gt 0 || {
    echo "not ok" 1 - "parallel decoder timeline omitted paint starts"
    exit 0
}
test "${paint_start}" -eq "${paint_finish}" || {
    echo "not ok" 1 - "parallel decoder paint spans are unpaired"
    exit 0
}

echo "ok" 1 - "parallel decoder timeline spans are paired"
exit 0
