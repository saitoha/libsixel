#!/bin/sh
# Policy: docs/functionality/or-mode.md
# Distinguish this OR composition rule from ordinary painting.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/0030_ormode_zero_mask_retains_bits" || {
    echo "not ok 1 - ormode_zero_mask_retains_bits"
    exit 0
}

echo "ok 1 - ormode_zero_mask_retains_bits"
exit 0
