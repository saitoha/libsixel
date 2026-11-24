#!/bin/sh
# Heckbert histogram half numerical contract.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/heckbert_policy/histogram_half" || {
    printf '\n%s\n' "not ok 1 - Heckbert histogram_half"
    exit 0
}
printf '\n%s\n' "ok 1 - Heckbert histogram_half"
exit 0
