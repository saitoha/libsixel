#!/bin/sh
# Verify bucket size first sweep.
# Policy: docs/encoder/output/encode-policy/README.md
set -eux

echo "1..1"
set -v

set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/pipeline/0073_bucket_size_first_sweep"
status=$?
set -e
test "$status" -ne 77 || {
    echo "ok 1 - bucket size first sweep # SKIP threads unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - bucket size first sweep"
    exit 0
}
echo "ok 1 - bucket size first sweep"
exit 0
