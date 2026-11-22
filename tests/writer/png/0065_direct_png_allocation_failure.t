#!/bin/sh
# Final PNG allocation failure releases both pixel and compression buffers.
set -eux

echo "1..1"
set -v

result=0
SIXEL_LOG_PATH='' ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    writer/png/0065_direct_png_allocation_failure || result=$?
test "$result" -ne 77 || {
    echo "ok 1 # SKIP builtin PNG writer is unavailable in this build"
    exit 0
}
test "$result" -eq 0 || {
    echo "not ok 1 - final PNG allocation failure releases compressed data"
    exit 0
}
echo "ok 1 - final PNG allocation failure releases compressed data"
exit 0
