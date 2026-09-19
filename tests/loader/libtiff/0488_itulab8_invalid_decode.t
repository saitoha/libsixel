#!/bin/sh
# Policy: docs/loader/color-management.md
# Verify itulab8 invalid decode with independent values.
set -eux
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0488_itulab8_invalid_decode 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP required loader or CMS engine is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - itulab8 invalid decode"
    exit 0
}
echo "ok 1 - itulab8 invalid decode"
exit 0
