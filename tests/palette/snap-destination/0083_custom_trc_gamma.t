#!/bin/sh
# Unnamed per-channel gamma model in gamma.
# Policy: docs/encoder/palette-construction/snap-policy/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/destination_snap/0001_destination grid 2 2 0 || {
    echo "not ok 1 - Unnamed per-channel gamma model in gamma"
    exit 0
}

echo "ok 1 - Unnamed per-channel gamma model in gamma"
exit 0
