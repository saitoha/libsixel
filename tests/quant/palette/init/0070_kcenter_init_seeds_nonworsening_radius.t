#!/bin/sh
# TAP test: init_seeds should not worsen the selected k-center radius.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0003_kcenter_constraints" init-seeds-nonworsening >/dev/null || {
    echo "not ok" 1 - "kcenter init_seeds non-worsening check failed"
    exit 0
}

echo "ok" 1 - "kcenter init_seeds non-worsening check passed"
exit 0
