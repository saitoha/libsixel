#!/bin/sh
# TAP test: CLARANS adaptive row-cache sizing keeps seed reproducibility.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0002_kmedoids_constraints" \
    clarans-adaptive-row-cache-seed-reproducibility \
    >/dev/null || {
    echo "not ok" 1 - "kmedoids CLARANS adaptive row-cache seed failed"
    exit 0
}

echo "ok" 1 - "kmedoids CLARANS adaptive row-cache seed passed"
exit 0
