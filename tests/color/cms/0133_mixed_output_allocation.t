#!/bin/sh
# Policy: docs/loader/color-management/README.md
# Verify mixed output allocation.
set -eux
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    cms/0361_mixed_output_allocation 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP ColorSync is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - mixed output allocation"
    exit 0
}
case "${trace}" in *'MDFAULT|case=1|'*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - fault qualification is absent"
    exit 0
}
echo "ok 1 - mixed output allocation"
exit 0
