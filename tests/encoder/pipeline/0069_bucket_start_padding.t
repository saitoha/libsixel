#!/bin/sh
# Verify bucket start padding.
# Policy: docs/encoder/output/encode-policy/README.md
set -eux

echo "1..1"
set -v

set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/pipeline/0069_bucket_start_padding"
status=$?
set -e
test "$status" -ne 77 || {
    echo "ok 1 - bucket start padding # SKIP threads unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - bucket start padding"
    exit 0
}
echo "ok 1 - bucket start padding"
exit 0
