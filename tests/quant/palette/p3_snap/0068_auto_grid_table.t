#!/bin/sh
# Verify auto grid table with independent P3 expectations.
# Policy: docs/encoder/palette-construction/snap-policy/README.md
set -eux

echo "1..1"
set -v

trace=$(SIXEL_TRACE_TOPIC=palette_contract ${SIXEL_RUNTIME-} \
    "${TEST_RUNNER_PATH}" palette/p3_snap/0001_grid 0 2 2>&1) || {
    echo "not ok 1 - auto grid values: $trace"
    exit 0
}
case "$trace" in
    *'LSXSG1|strategy=2|queries='[1-9]*) ;;
    *) echo "not ok 1 - auto did not execute table queries"; exit 0 ;;
esac
echo "ok 1 - auto grid table"
exit 0
