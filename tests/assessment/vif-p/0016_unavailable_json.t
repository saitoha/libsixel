#!/bin/sh
# Policy: docs/quality/metrics.md
# VIF-P unavailable JSON retains other metric results.
set -eux
echo "1..1"
set -v

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none \
    -W srgb -P float32 -m VIF-P,GMSD \
    "${TOP_SRCDIR}/tests/data/assessment/vif-p-flat.pgm" "${TOP_SRCDIR}/tests/data/assessment/vif-p-flat.pgm") || {
    printf '\n%s\n' "not ok 1 - VIF-P unavailable JSON"
    exit 0
}
test "${value}" = '{
  "VIF-P": null,
  "GMSD": 0.000000
}' || {
    printf '\n%s\n' "not ok 1 - VIF-P unavailable JSON"
    exit 0
}
printf '\n%s\n' "ok 1 - VIF-P unavailable JSON retains other metric results"
exit 0
