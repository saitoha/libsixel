#!/bin/sh
# Indexed compression failure releases the filtered rows and the output.
set -eux

echo "1..1"
set -v

result=0
SIXEL_LOG_PATH='' ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    writer/png/0066_indexed_compression_failure || result=$?
test "$result" -ne 77 || {
    printf '\n%s\n' "ok 1 # SKIP builtin PNG writer is unavailable in this build"
    exit 0
}
test "$result" -eq 0 || {
    printf '\n%s\n' "not ok 1 - indexed compression failure releases the rows and output"
    exit 0
}
printf '\n%s\n' "ok 1 - indexed compression failure releases the rows and output"
exit 0
