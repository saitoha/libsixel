#!/bin/sh
# Policy: docs/quality/metrics.md
# A finite reference score below the configured baseline reports quality failure.
set -eux
echo "1..1"
set -v

status=0
value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none \
    -m MS-SSIM -b ms-ssim:1.01 \
    "${TOP_SRCDIR}/images/snake.png" "${TOP_SRCDIR}/images/snake.png") || status=$?
test "${status}" -eq 5 && test "${value}" = '1.000000' || {
    echo "not ok 1 - finite reference baseline failure"
    exit 0
}
echo "ok 1 - finite reference baseline failure"
exit 0
