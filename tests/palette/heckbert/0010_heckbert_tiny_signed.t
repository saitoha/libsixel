#!/bin/sh
# Verify native Heckbert tiny signed.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/heckbert_float/0010_heckbert_tiny_signed >/dev/null || {
    printf '\n%s\n' "not ok 1 - native Heckbert tiny signed"
    exit 0
}

printf '\n%s\n' "ok 1 - native Heckbert tiny signed"
exit 0
