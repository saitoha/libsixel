#!/bin/sh
# Heckbert once snap runs only on the final palette.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/ordinary_float/heckbert_once" || {
    printf '\n%s\n' "not ok 1 - Heckbert once snap runs only on the final palette"
    exit 0
}
printf '\n%s\n' "ok 1 - Heckbert once snap runs only on the final palette"
exit 0
