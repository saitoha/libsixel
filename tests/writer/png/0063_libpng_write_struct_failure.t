#!/bin/sh
# A failed libpng writer creation reports an error and closes the output.
set -eux

echo "1..1"
set -v

result=0
SIXEL_LOG_PATH='' ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    writer/png/0063_libpng_write_struct_failure || result=$?
test "$result" -ne 77 || {
    echo "ok 1 # SKIP libpng writer is unavailable in this build"
    exit 0
}
test "$result" -eq 0 || {
    echo "not ok 1 - libpng writer creation failure closes the output"
    exit 0
}
echo "ok 1 - libpng writer creation failure closes the output"
exit 0
