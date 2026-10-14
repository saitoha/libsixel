#!/bin/sh
# Verify failed option preserves history.
set -eux

echo "1..1"
set -v

set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0011_failed_option_preserves_history"
result=$?
set -e
test "$result" -ne 77 || {
    echo "ok 1 - failed option preserves history # SKIP thread support disabled"
    exit 0
}
test "$result" -eq 0 || {
    echo "not ok 1 - failed option preserves history"
    exit 0
}
echo "ok 1 - failed option preserves history"
exit 0
