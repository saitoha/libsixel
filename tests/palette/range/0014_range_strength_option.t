#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# Verify range strength option.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/adaptive_range/0014_range_strength_option" || {
    echo "not ok 1 - 0014_range_strength_option"
    exit 0
}
echo "ok 1 - 0014_range_strength_option"
exit 0
