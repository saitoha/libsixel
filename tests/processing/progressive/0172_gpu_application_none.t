#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Match CPU zero-noise masks on sparse offset input.
set -eux
echo "1..1"
set -v
set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0172_gpu_application_none"
result=$?
set -e
test "$result" != 77 || {
    echo "ok 1 - gpu application none # SKIP Metal device unavailable"
    exit 0
}
test "$result" = 0 || {
    echo "not ok 1 - gpu application none"
    exit 0
}
echo "ok 1 - gpu application none"
exit 0
