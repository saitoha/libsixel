#!/bin/sh
# TAP test: auto_policy=adaptive should select fft/hybrid by threshold branches.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0003_kcenter_constraints" auto-policy-adaptive-selection >/dev/null || {
    echo "not ok" 1 - "kcenter auto_policy adaptive branch check failed"
    exit 0
}

echo "ok" 1 - "kcenter auto_policy adaptive branch check passed"
exit 0
