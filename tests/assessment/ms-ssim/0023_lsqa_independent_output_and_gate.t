#!/bin/sh
# Policy: docs/quality/metrics.md
# Output selection and gate metrics are independent
set -eux
echo "1..1"
set -v

status=0
value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none -m PSNR_Y -b GMSD:max=0 "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" "${TOP_SRCDIR}/tests/data/inputs/snake_16.png") || status=$?

test "${status}" -eq 0 && test "${value}" = 99.000000 || {
    echo "not ok 1 - Output selection and gate metrics are independent"
    exit 0
}
echo "ok 1 - Output selection and gate metrics are independent"
exit 0
