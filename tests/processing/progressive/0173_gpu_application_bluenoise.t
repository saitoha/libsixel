#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Preserve absolute noise phase, protected pixels and keep masks.
set -eux
echo "1..1"
set -v
set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0173_gpu_application_bluenoise"
result=$?
set -e
test "$result" != 77 || {
    echo "ok 1 - gpu application bluenoise # SKIP Metal device unavailable"
    exit 0
}
test "$result" = 0 || {
    echo "not ok 1 - gpu application bluenoise"
    exit 0
}
echo "ok 1 - gpu application bluenoise"
exit 0
