#!/bin/sh
# Verify bucket empty first band.
# Policy: docs/encoder/output/encode-policy/README.md
set -eux

echo "1..1"
set -v

set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/pipeline/0070_bucket_empty_first_band"
status=$?
set -e
test "$status" -ne 77 || {
    echo "ok 1 - bucket empty first band # SKIP threads unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - bucket empty first band"
    exit 0
}
echo "ok 1 - bucket empty first band"
exit 0
