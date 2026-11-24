#!/bin/sh
# Verify destination snap: pipeline heckbert full.
# Policy: docs/encoder/palette-construction/snap-policy/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/p3_snap/0002_pipeline 3 2 0 0 || {
    printf '\n%s\n' "not ok 1 - P3 snap pipeline heckbert full"
    exit 0
}

printf '\n%s\n' "ok 1 - P3 snap pipeline heckbert full"
exit 0
