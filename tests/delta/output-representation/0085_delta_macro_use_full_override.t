#!/bin/sh
# Explicit full output clears delta history and allows automatic macros.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0228_delta_macro_use_full_override || {
    echo "not ok 1 - macro use full override"
    exit 0
}
echo "ok 1 - macro use full override"
exit 0
