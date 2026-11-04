#!/bin/sh
# Verify bucket size partial rows.
# Policy: docs/encoder/output/encode-policy/README.md
set -eux

echo "1..1"
set -v

set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/pipeline/0074_bucket_size_partial_rows"
status=$?
set -e
test "$status" -ne 77 || {
    echo "ok 1 - bucket size partial rows # SKIP threads unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - bucket size partial rows"
    exit 0
}
echo "ok 1 - bucket size partial rows"
exit 0
