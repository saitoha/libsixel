#!/bin/sh
# Preservation retains the explicitly requested final partial snap.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/ordinary_float/center_snap" || {
    printf '\n%s\n' "not ok 1 - Preservation retains the explicitly requested final partial snap"
    exit 0
}
printf '\n%s\n' "ok 1 - Preservation retains the explicitly requested final partial snap"
exit 0
