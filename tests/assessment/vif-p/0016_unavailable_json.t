#!/bin/sh
# Policy: docs/quality/metrics.md
# VIF-P unavailable JSON retains other metric results.
set -eux
echo "1..1"
set -v

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none \
    -W gamma -P float32 -m VIF-P,GMSD \
    "${TOP_SRCDIR}/tests/data/assessment/vif-p-flat.pgm" "${TOP_SRCDIR}/tests/data/assessment/vif-p-flat.pgm") || {
    echo "not ok 1 - VIF-P unavailable JSON"
    exit 0
}
test "${value}" = '{
  "VIF-P": null,
  "GMSD": 0.000000
}' || {
    echo "not ok 1 - VIF-P unavailable JSON"
    exit 0
}
echo "ok 1 - VIF-P unavailable JSON retains other metric results"
exit 0
