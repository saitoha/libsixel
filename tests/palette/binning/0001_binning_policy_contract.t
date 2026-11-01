#!/bin/sh
# Verify independent binning policy resolution and allocation bounds.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0030_binning_policy_contract" || {
    echo "not ok 1 - binning policy contract"
    exit 0
}

echo "ok 1 - binning policy contract"
exit 0
