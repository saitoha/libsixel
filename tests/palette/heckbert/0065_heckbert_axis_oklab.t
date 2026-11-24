#!/bin/sh
# Heckbert axis oklab numerical contract.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/heckbert_policy/axis_oklab" || {
    printf '\n%s\n' "not ok 1 - Heckbert axis_oklab"
    exit 0
}
printf '\n%s\n' "ok 1 - Heckbert axis_oklab"
exit 0
