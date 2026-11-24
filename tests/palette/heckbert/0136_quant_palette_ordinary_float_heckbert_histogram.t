#!/bin/sh
# Heckbert occurrence mean retains float values.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/ordinary_float/heckbert_histogram" || {
    printf '\n%s\n' "not ok 1 - Heckbert occurrence mean retains float values"
    exit 0
}
printf '\n%s\n' "ok 1 - Heckbert occurrence mean retains float values"
exit 0
