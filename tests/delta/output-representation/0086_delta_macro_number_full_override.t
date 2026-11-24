#!/bin/sh
# Explicit full output clears delta history and allows fixed-ID macros.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0229_delta_macro_number_full_override || {
    printf '\n%s\n' "not ok 1 - macro number full override"
    exit 0
}
printf '\n%s\n' "ok 1 - macro number full override"
exit 0
