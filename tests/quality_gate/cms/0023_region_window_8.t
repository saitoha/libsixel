#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# Verify region window 8.
set -eux
echo "1..1"
set -v

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m DE00_region_mean -R display-p3 -T display-p3 -w 8 "${TOP_SRCDIR}/tests/data/color-numeric/red.six" "${TOP_SRCDIR}/tests/data/color-numeric/red.six") || {
    echo "not ok 1 - evaluation failed"
    exit 0
}
test "${value%%[!0-9.]*}" = 0.000000 || {
    echo "not ok 1 - unexpected numeric color difference"
    exit 0
}
echo "ok 1 - region window 8"
exit 0
