#!/bin/sh
# TAP test: auto+perceptual+OKLab should prefer hybrid over fft.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0003_kcenter_constraints" \
    auto-perceptual-oklab-hybrid-preference >/dev/null || {
    echo "not ok" 1 - "kcenter auto perceptual OKLab preference check failed"
    exit 0
}

echo "ok" 1 - "kcenter auto perceptual OKLab preference check passed"
exit 0
