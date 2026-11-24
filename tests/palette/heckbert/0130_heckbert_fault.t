#!/bin/sh
# Heckbert allocation failure cannot retry byte storage.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/ordinary_float/heckbert_fault" || {
    printf '\n%s\n' "not ok 1 - Heckbert allocation failure cannot retry byte storage"
    exit 0
}
printf '\n%s\n' "ok 1 - Heckbert allocation failure cannot retry byte storage"
exit 0
