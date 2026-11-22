#!/bin/sh
# A builtin ICC compression failure releases the exported profile.
set -eux

echo "1..1"
set -v

result=0
SIXEL_LOG_PATH='' ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    writer/png/0061_icc_compression_failure || result=$?
test "$result" -ne 77 || {
    echo "ok 1 # SKIP builtin PNG writer is disabled in this build"
    exit 0
}
test "$result" -eq 0 || {
    echo "not ok 1 - ICC compression failure reports its diagnostic and releases the profile"
    exit 0
}
echo "ok 1 - ICC compression failure reports its diagnostic and releases the profile"
exit 0
