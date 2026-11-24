#!/bin/sh
# Policy: docs/quality/metrics.md
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none -b S-CIEDE2000:max=0 "${TOP_SRCDIR}/tests/data/inputs/snake_16.bmp" "${TOP_SRCDIR}/tests/data/inputs/snake_16.jpg" >/dev/null || status=$?
test "${status}" -eq 5 || {
    printf '\n%s\n' "not ok 1 - spatial DE00 mean ceiling"
    exit 0
}

printf '\n%s\n' "ok 1 - spatial DE00 mean ceiling rejects distortion"
exit 0
