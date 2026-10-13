#!/bin/sh
# Live destination caches own distinct transforms.
# Policy: docs/encoder/palette-construction/snap-policy/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/destination_snap/0001_destination isolation || {
    echo "not ok 1 - Live destination caches own distinct transforms"
    exit 0
}

echo "ok 1 - Live destination caches own distinct transforms"
exit 0
