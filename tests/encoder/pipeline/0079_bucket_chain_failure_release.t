#!/bin/sh
# Verify bucket chain failure release.
# Policy: docs/threading/encoder.md
set -eux

echo "1..1"
set -v

set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/pipeline/0079_bucket_chain_failure_release"
status=$?
set -e
test "$status" -ne 77 || {
    printf '\n%s\n' "ok 1 - bucket chain failure release # SKIP threads unavailable"
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - bucket chain failure release"
    exit 0
}
printf '\n%s\n' "ok 1 - bucket chain failure release"
exit 0
