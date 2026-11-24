#!/bin/sh
# Verify delta a source.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0018_delta_a_source || {
    printf '\n%s\n' "not ok 1 - delta a source"
    exit 0
}
printf '\n%s\n' "ok 1 - delta a source"
exit 0
