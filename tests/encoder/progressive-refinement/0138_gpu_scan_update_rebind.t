#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Reject stale GPU scan work after an ordinary update and rebind safely.
set -eux
echo "1..1"
set -v
set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0190_gpu_scan_update_rebind"
result=$?
set -e
test "$result" != 77 || {
    printf '\n%s\n' "ok 1 - gpu scan update rebind # SKIP GPU device unavailable"
    exit 0
}
test "$result" = 0 || {
    printf '\n%s\n' "not ok 1 - gpu scan update rebind"
    exit 0
}
printf '\n%s\n' "ok 1 - gpu scan update rebind"
exit 0
