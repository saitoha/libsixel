#!/bin/sh
# Policy: docs/loader/color-management/README.md
# Verify lab16 planar late read with independent values.
set -eux
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0489_lab16_planar_late_read 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP required loader or CMS engine is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - lab16 planar late read"
    exit 0
}
echo "ok 1 - lab16 planar late read"
exit 0
