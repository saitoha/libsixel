#!/bin/sh
# Verify CLI terminal color transport noncanonical.
set -eux

result=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cli/0038_cli_color_tty \
    noncanonical || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP scripted POSIX terminal I/O is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "not ok 1 - terminal color transport noncanonical"
    exit 0
}
echo "ok 1 - terminal color transport noncanonical"
exit 0
