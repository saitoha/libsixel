#!/bin/sh
# Verify edge soft gamma.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/binning_float/0001_binning_contract \
    edge 0 2 0 || {
    printf '\n%s\n' "not ok 1 - edge soft gamma"
    exit 0
}
printf '\n%s\n' "ok 1 - edge soft gamma"
exit 0
