#!/bin/sh
# P3 DRCS delta rejects before output; full reuse retains native P3 application and endpoint values.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

result=0
SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0221_delta_drcs_p3_recovery || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP DRCS geometry backend is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v

test "$result" -eq 0 || {
    echo "not ok 1 - delta drcs p3 recovery"
    exit 0
}
echo "ok 1 - delta drcs p3 recovery"
exit 0
