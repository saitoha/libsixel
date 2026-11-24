#!/bin/sh
# A failed libpng info allocation destroys the partial writer.
set -eux

echo "1..1"
set -v

result=0
SIXEL_LOG_PATH='' ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    writer/png/0064_libpng_info_struct_failure || result=$?
test "$result" -ne 77 || {
    printf '\n%s\n' "ok 1 # SKIP libpng writer is unavailable in this build"
    exit 0
}
test "$result" -eq 0 || {
    printf '\n%s\n' "not ok 1 - libpng info allocation failure destroys the partial writer"
    exit 0
}
printf '\n%s\n' "ok 1 - libpng info allocation failure destroys the partial writer"
exit 0
