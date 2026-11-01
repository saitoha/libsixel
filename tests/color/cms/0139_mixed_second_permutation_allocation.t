#!/bin/sh
# Policy: docs/loader/color-management/README.md
# Verify mixed second permutation allocation.
set -eux
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    cms/0367_mixed_second_permutation_allocation 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP ColorSync is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - mixed second permutation allocation"
    exit 0
}
case "${trace}" in *'MDFAULT|case=7|'*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - fault qualification is absent"
    exit 0
}
echo "ok 1 - mixed second permutation allocation"
exit 0
