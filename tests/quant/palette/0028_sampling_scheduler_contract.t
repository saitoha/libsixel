#!/bin/sh
# Verify scheduler allocation consumes the effective sampling policy.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0028_sampling_scheduler_contract" || {
    echo "not ok 1 - sampling scheduler contract"
    exit 0
}

echo "ok 1 - sampling scheduler contract"
exit 0
