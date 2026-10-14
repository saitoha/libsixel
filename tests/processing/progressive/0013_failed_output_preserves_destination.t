#!/bin/sh
# Verify failed output preserves destination.
set -eux

echo "1..1"
set -v

set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0013_failed_output_preserves_destination"
result=$?
set -e
test "$result" -ne 77 || {
    echo "ok 1 - failed output preserves destination # SKIP thread support disabled"
    exit 0
}
test "$result" -eq 0 || {
    echo "not ok 1 - failed output preserves destination"
    exit 0
}
echo "ok 1 - failed output preserves destination"
exit 0
