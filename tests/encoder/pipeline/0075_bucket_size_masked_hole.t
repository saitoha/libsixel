#!/bin/sh
# Verify bucket size masked hole.
# Policy: docs/encoder/output/encode-policy/README.md
set -eux

echo "1..1"
set -v

set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/pipeline/0075_bucket_size_masked_hole"
status=$?
set -e
test "$status" -ne 77 || {
    echo "ok 1 - bucket size masked hole # SKIP threads unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - bucket size masked hole"
    exit 0
}
echo "ok 1 - bucket size masked hole"
exit 0
