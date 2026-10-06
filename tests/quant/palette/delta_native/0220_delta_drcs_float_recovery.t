#!/bin/sh
# Float DRCS delta rejects before palette work and output; full reuse retains native float application.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

result=0
SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0220_delta_drcs_float_recovery || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP DRCS geometry backend is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v

test "$result" -eq 0 || {
    echo "not ok 1 - delta drcs float recovery"
    exit 0
}
echo "ok 1 - delta drcs float recovery"
exit 0
