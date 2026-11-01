#!/bin/sh
# Verify loader callback reentry.
set -eux

echo "1..1"
set -v

set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0015_loader_callback_reentry" \
    "${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgba_loop2.png"
result=$?
set -e
test "$result" -ne 77 || {
    echo "ok 1 - loader callback reentry # SKIP thread support disabled"
    exit 0
}
test "$result" -eq 0 || {
    echo "not ok 1 - loader callback reentry"
    exit 0
}
echo "ok 1 - loader callback reentry"
exit 0
