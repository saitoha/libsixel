#!/bin/sh
# Policy: docs/encoder/output/or-mode/README.md
# Distinguish this OR composition rule from ordinary painting.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/0030_ormode_zero_mask_retains_bits" || {
    printf '\n%s\n' "not ok 1 - ormode_zero_mask_retains_bits"
    exit 0
}

printf '\n%s\n' "ok 1 - ormode_zero_mask_retains_bits"
exit 0
