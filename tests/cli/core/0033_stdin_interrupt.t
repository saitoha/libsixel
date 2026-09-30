#!/bin/sh
# Verify Ctrl-C stops terminal stdin after an input line has been consumed.
# Policy: docs/cli/design-policy.md
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel unavailable"
    set -v
    exit 0
}

result=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cli/core/0033_stdin_interrupt \
    "${IMG2SIXEL_PATH}" || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP POSIX terminal input inspection unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "not ok 1 - Ctrl-C stops terminal stdin after Enter"
    exit 0
}
echo "ok 1 - Ctrl-C stops terminal stdin after Enter"
exit 0
