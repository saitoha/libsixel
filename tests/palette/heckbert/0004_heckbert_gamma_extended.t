#!/bin/sh
# Verify native Heckbert gamma extended.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/heckbert_float/0004_heckbert_gamma_extended >/dev/null || {
    printf '\n%s\n' "not ok 1 - native Heckbert gamma extended"
    exit 0
}

printf '\n%s\n' "ok 1 - native Heckbert gamma extended"
exit 0
