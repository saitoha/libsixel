#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# Verify scheduler allocation consumes the effective sampling policy.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0028_sampling_scheduler_contract" || {
    printf '\n%s\n' "not ok 1 - sampling scheduler contract"
    exit 0
}

printf '\n%s\n' "ok 1 - sampling scheduler contract"
exit 0
