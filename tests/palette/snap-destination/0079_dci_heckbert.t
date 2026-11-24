#!/bin/sh
# dci heckbert snap pipeline values.
# Policy: docs/encoder/palette-construction/snap-policy/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/destination_snap/0001_destination pipeline 1 3 1 0 0 || {
    printf '\n%s\n' "not ok 1 - dci heckbert snap pipeline values"
    exit 0
}

printf '\n%s\n' "ok 1 - dci heckbert snap pipeline values"
exit 0
