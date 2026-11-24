#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# Verify region window 64.
set -eux
echo "1..1"
set -v

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m DE00_region_mean -R display-p3 -T display-p3 -w 64 "${TOP_SRCDIR}/tests/data/color-numeric/red.six" "${TOP_SRCDIR}/tests/data/color-numeric/red.six") || {
    printf '\n%s\n' "not ok 1 - evaluation failed"
    exit 0
}
test "${value%%[!0-9.]*}" = 0.000000 || {
    printf '\n%s\n' "not ok 1 - unexpected numeric color difference"
    exit 0
}
printf '\n%s\n' "ok 1 - region window 64"
exit 0
