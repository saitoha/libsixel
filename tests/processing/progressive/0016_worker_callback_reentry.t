#!/bin/sh
# Verify worker callback reentry.
set -eux

echo "1..1"
set -v

set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0016_worker_callback_reentry" \
    "${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgba_loop2.png"
result=$?
set -e
test "$result" -ne 77 || {
    echo "ok 1 - worker callback reentry # SKIP thread support disabled"
    exit 0
}
test "$result" -eq 0 || {
    echo "not ok 1 - worker callback reentry"
    exit 0
}
echo "ok 1 - worker callback reentry"
exit 0
