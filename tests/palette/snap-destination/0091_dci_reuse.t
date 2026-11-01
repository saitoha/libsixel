#!/bin/sh
# Active snap rebuilds its definition across dci encoder reuse.
# Policy: docs/encoder/palette-construction/snap-policy/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/destination_snap/0001_destination pipeline 1 0 1 0 5 || {
    echo "not ok 1 - Active snap rebuilds its definition across dci encoder reuse"
    exit 0
}

echo "ok 1 - Active snap rebuilds its definition across dci encoder reuse"
exit 0
