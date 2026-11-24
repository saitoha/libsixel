#!/bin/sh
# Selecting delta after automatic macro output preserves options and history on rejection.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0222_delta_macro_use_then_delta || {
    printf '\n%s\n' "not ok 1 - macro use then delta"
    exit 0
}
printf '\n%s\n' "ok 1 - macro use then delta"
exit 0
