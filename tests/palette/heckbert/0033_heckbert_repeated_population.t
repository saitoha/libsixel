#!/bin/sh
# Repeated colors retain exact populations within a compact budget.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/heckbert_float/0033_heckbert_repeated_population || {
    printf '\n%s\n' "not ok 1 - native Heckbert repeated_population"
    exit 0
}
printf '\n%s\n' "ok 1 - native Heckbert repeated_population"
exit 0
