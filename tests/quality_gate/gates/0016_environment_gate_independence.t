#!/bin/sh
# Policy: docs/quality/metrics.md
# CLI output selection retains environment gates
set -eux
echo "1..1"
set -v

status=0
value=$(LSQA_BASELINE='GMSD:max=-1' ${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none -m PSNR_Y "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" "${TOP_SRCDIR}/tests/data/inputs/snake_16.png") || status=$?

test "${status}" -eq 5 && test "${value}" = 99.000000 || {
    echo "not ok 1 - CLI output selection retains environment gates"
    exit 0
}
echo "ok 1 - CLI output selection retains environment gates"
exit 0
