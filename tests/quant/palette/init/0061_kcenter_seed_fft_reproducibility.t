#!/bin/sh
# TAP test: k-center seed reproducibility for fft.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0003_kcenter_constraints" seed-fft >/dev/null || {
    echo "not ok" 1 - "kcenter seed reproducibility failed for fft"
    exit 0
}

echo "ok" 1 - "kcenter seed reproducibility passed for fft"
exit 0
