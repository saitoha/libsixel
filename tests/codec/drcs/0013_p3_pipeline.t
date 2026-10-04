#!/bin/sh
# Verify native DRCS p3 pipeline.
# Policy: docs/encoder/output/drcs.md
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP thread backend is unavailable"
    set -v
    exit 0
}

result=0
SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    codec/drcs/0013_p3_pipeline || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP DRCS geometry backend is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v

test "$result" -eq 0 || {
    echo "not ok 1 - native DRCS p3 pipeline"
    exit 0
}
echo "ok 1 - native DRCS p3 pipeline"
exit 0
