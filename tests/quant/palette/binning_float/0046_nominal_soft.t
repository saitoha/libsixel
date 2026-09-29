#!/bin/sh
# Verify nominal soft.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/binning_float/0001_binning_contract \
    nominal 0 2 0 || {
    echo "not ok 1 - nominal soft"
    exit 0
}
echo "ok 1 - nominal soft"
exit 0
