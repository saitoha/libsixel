#!/bin/sh
# Verify failed drcs preserves mode.
set -eux

echo "1..1"
set -v

set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0014_failed_drcs_preserves_mode"
result=$?
set -e
test "$result" -ne 77 || {
    printf '\n%s\n' "ok 1 - failed drcs preserves mode # SKIP thread support disabled"
    exit 0
}
test "$result" -eq 0 || {
    printf '\n%s\n' "not ok 1 - failed drcs preserves mode"
    exit 0
}
printf '\n%s\n' "ok 1 - failed drcs preserves mode"
exit 0
