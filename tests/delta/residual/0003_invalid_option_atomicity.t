#!/bin/sh
# Verify invalid option atomicity.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" delta-residual/0003_invalid_option_atomicity || {
    printf '\n%s\n' "not ok 1 - invalid option atomicity"
    exit 0
}
printf '\n%s\n' "ok 1 - invalid option atomicity"
exit 0
