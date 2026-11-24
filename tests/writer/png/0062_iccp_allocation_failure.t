#!/bin/sh
# A builtin iCCP allocation failure releases the compressed profile.
set -eux

echo "1..1"
set -v

result=0
SIXEL_LOG_PATH='' ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    writer/png/0062_iccp_allocation_failure || result=$?
test "$result" -ne 77 || {
    printf '\n%s\n' "ok 1 # SKIP builtin PNG writer is disabled in this build"
    exit 0
}
test "$result" -eq 0 || {
    printf '\n%s\n' "not ok 1 - iCCP allocation failure reports its diagnostic and releases the compressed profile"
    exit 0
}
printf '\n%s\n' "ok 1 - iCCP allocation failure reports its diagnostic and releases the compressed profile"
exit 0
