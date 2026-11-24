#!/bin/sh
# Verify bucket greedy successor.
# Policy: docs/encoder/output/encode-policy/README.md
set -eux

echo "1..1"
set -v

set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/pipeline/0067_bucket_greedy_successor"
status=$?
set -e
test "$status" -ne 77 || {
    printf '\n%s\n' "ok 1 - bucket greedy successor # SKIP threads unavailable"
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - bucket greedy successor"
    exit 0
}
printf '\n%s\n' "ok 1 - bucket greedy successor"
exit 0
