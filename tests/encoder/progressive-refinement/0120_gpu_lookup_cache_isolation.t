#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Isolate owners and refresh reused query and palette addresses.
set -eux
echo "1..1"
set -v
set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0170_gpu_lookup_cache_isolation"
result=$?
set -e
test "$result" != 77 || {
    printf '\n%s\n' "ok 1 - gpu lookup cache isolation # SKIP GPU device unavailable"
    exit 0
}
test "$result" = 0 || {
    printf '\n%s\n' "not ok 1 - gpu lookup cache isolation"
    exit 0
}
printf '\n%s\n' "ok 1 - gpu lookup cache isolation"
exit 0
