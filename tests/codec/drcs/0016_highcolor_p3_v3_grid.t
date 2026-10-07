#!/bin/sh
# Verify highcolor p3 v3 grid.
# Policy: docs/encoder/output/drcs.md
set -eux

result=0
SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    codec/drcs/0016_highcolor_p3_v3_grid || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP DRCS geometry backend is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v

test "$result" -eq 0 || {
    echo "not ok 1 - highcolor p3 v3 grid"
    exit 0
}
echo "ok 1 - highcolor p3 v3 grid"
exit 0
