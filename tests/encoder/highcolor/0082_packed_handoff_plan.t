#!/bin/sh
# Packed normalization requests a shared-frame copy.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0082_packed_handoff_plan || {
    printf '\n%s\n' "not ok 1 - high-color packed handoff plan"
    exit 0
}
printf '\n%s\n' "ok 1 - high-color packed handoff plan"
exit 0
