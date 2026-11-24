#!/bin/sh
# Verify dependent option atomicity.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" delta-residual/0004_dependent_option_atomicity || {
    printf '\n%s\n' "not ok 1 - dependent option atomicity"
    exit 0
}
printf '\n%s\n' "ok 1 - dependent option atomicity"
exit 0
