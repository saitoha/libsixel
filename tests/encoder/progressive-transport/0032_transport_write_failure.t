#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify transport write failure.
set -eux

echo "1..1"
set -v

set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0115_transport_write_failure"
result=$?
set -e
test "$result" -ne 77 || {
    printf '\n%s\n' "ok 1 - transport write failure # SKIP thread support disabled"
    exit 0
}
test "$result" -eq 0 || {
    printf '\n%s\n' "not ok 1 - transport write failure"
    exit 0
}
printf '\n%s\n' "ok 1 - transport write failure"
exit 0
