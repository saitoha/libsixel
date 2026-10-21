#!/bin/sh
# Policy: docs/quality/metrics.md
# Empty metric member is rejected
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none -m GMSD,,PSNR_Y "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" >/dev/null 2>&1 || status=$?

test "${status}" -eq 2 || {
    echo "not ok 1 - Empty metric member is rejected"
    exit 0
}
echo "ok 1 - Empty metric member is rejected"
exit 0
