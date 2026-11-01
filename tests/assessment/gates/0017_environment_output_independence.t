#!/bin/sh
# Policy: docs/quality/metrics.md
# CLI gates retain environment output selection
set -eux
echo "1..1"
set -v

status=0
value=$(LSQA_METRICS=PSNR_Y ${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none -b GMSD:max=0 "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" "${TOP_SRCDIR}/tests/data/inputs/snake_16.png") || status=$?

test "${status}" -eq 0 && test "${value}" = 99.000000 || {
    echo "not ok 1 - CLI gates retain environment output selection"
    exit 0
}
echo "ok 1 - CLI gates retain environment output selection"
exit 0
