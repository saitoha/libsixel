#!/bin/sh
# Verify native DRCS p3 c1 body.
# Policy: docs/encoder/output/drcs.md
set -eux

result=0
SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    codec/drcs/0003_p3_c1_body || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP DRCS geometry backend is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v

test "$result" -eq 0 || {
    echo "not ok 1 - native DRCS p3 c1 body"
    exit 0
}
echo "ok 1 - native DRCS p3 c1 body"
exit 0
