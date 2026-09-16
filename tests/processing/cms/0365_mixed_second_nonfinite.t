#!/bin/sh
# Policy: docs/loader/color-management.md
# Verify mixed second nonfinite.
set -eux
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    cms/0365_mixed_second_nonfinite 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP ColorSync is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - mixed second nonfinite"
    exit 0
}
test "${trace#*'MDFAULT|case=5|'}" != "$trace" || {
    echo "$trace" >&2
    echo "not ok 1 - fault qualification is absent"
    exit 0
}
echo "ok 1 - mixed second nonfinite"
exit 0
