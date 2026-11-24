#!/bin/sh
# Repeated sub-byte colors and signed zero retain distinct entries.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/heckbert_float/0034_heckbert_repeated_subbyte || {
    printf '\n%s\n' "not ok 1 - native Heckbert repeated_subbyte"
    exit 0
}
printf '\n%s\n' "ok 1 - native Heckbert repeated_subbyte"
exit 0
