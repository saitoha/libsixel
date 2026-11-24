#!/bin/sh
# The PNG file frontend rejects delta before input/staging access and preserves its destination and history.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    codec/png_snapshot/0022_delta_file_guard || {
    printf '\n%s\n' "not ok 1 - PNG delta file guard"
    exit 0
}
printf '\n%s\n' "ok 1 - PNG delta file guard"
exit 0
