#!/bin/sh
# Selecting a fixed macro ID after delta rejects without installing the ID.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0225_delta_delta_then_macro_number || {
    printf '\n%s\n' "not ok 1 - delta then macro number"
    exit 0
}
printf '\n%s\n' "ok 1 - delta then macro number"
exit 0
